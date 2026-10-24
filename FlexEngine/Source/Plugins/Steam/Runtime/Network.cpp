#include "Plugins/Steam/Runtime/Network.h"
#include "Plugins/Steam/Runtime/PacketRegistery.h"
#include "Plugins/Steam/Runtime/SteamInternal.h"
#include "Plugins/Steam/Runtime/SteamLobby.h"
#include "Plugins/Steam/Runtime/SteamSdk.h"
#include "Plugins/Steam/Runtime/SteamSession.h"
#include "Core/Game/GameModule.h"

#include <SDL3/SDL.h>
#include <timeapi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace Flex::Steam
{
	namespace
	{
		// Wire format of a message: [Route][extra][PacketTypeId][payload] (see EncodePacket).
		enum class Route : uint8_t
		{
			Direct = 0,        // for the receiver itself
			RelayToOthers = 1, // client -> host: for the host and every other client; extra: none
			RelayToUser = 2,   // client -> host: for one member; extra: uint64 target
			Relayed = 3        // host -> client: passed on from another client; extra: uint64 origin
		};

		constexpr auto PendingTimeout = std::chrono::seconds(10); // host: wait this long for a connecting user to show up in the lobby
		constexpr int ReceiveBatch = 64;

		struct OutgoingPacket
		{
			PacketHandle Packet;
			Target To;
			Delivery Mode = Delivery::Reliable;
		};

		struct InboxPacket
		{
			PacketHandle Packet;
			PacketContext Context;
		};

		struct PendingConnection
		{
			HSteamNetConnection Connection = k_HSteamNetConnection_Invalid;
			uint64_t User = 0;
			std::chrono::steady_clock::time_point Since;
		};

		// Who's connected, as the network thread sees it (copied when Version changes).
		struct PeerSnapshot
		{
			bool bIsHost = false;
			uint64_t Self = 0;
			uint64_t Host = 0;
			HSteamNetConnection HostConnection = k_HSteamNetConnection_Invalid;
			HSteamNetPollGroup PollGroup = k_HSteamNetPollGroup_Invalid;
			std::unordered_map<uint64_t, HSteamNetConnection> Clients;          // host: user -> connection
			std::unordered_map<HSteamNetConnection, uint64_t> ConnectionUsers;  // connection -> user
		};

		class ConnectionCallbacks
		{
		public:
			STEAM_CALLBACK(ConnectionCallbacks, OnConnectionStatus, SteamNetConnectionStatusChangedCallback_t);
		};

		struct NetworkData
		{
			// Peers: changed by the main thread only, read by the network thread under the mutex.
			std::mutex PeersMutex;
			PeerSnapshot Peers;
			std::atomic<uint32_t> PeersVersion = 0;
			std::atomic<bool> bActive = false;
			std::atomic<bool> bHostConnected = false;
			HSteamListenSocket ListenSocket = k_HSteamListenSocket_Invalid;
			std::vector<PendingConnection> Pending; // main thread only

			std::mutex QueueMutex;
			std::condition_variable Wake;
			std::vector<OutgoingPacket> SendQueue;

			std::mutex InboxMutex;
			std::vector<InboxPacket> Inbox;

			// Held by the network thread while it handles packets, so hot reload can wait for it.
			std::mutex ProcessingMutex;
			std::thread* Thread = nullptr; // deliberately leaked if the process exits during a session
			std::atomic<bool> bStopThread = false;

			std::mutex StatsMutex;
			NetworkStats Totals;
			std::unordered_map<PacketTypeId, PacketTypeStats> TypeStats;

			std::unique_ptr<ConnectionCallbacks> Callbacks;
			uint32_t GameModuleListener = 0;
		};
		NetworkData Data;

		ISteamNetworkingSockets* Sockets()
		{
			return SteamNetworkingSockets();
		}

		int ToSendFlags(Delivery delivery)
		{
			switch (delivery) {
			case Delivery::Unreliable: return k_nSteamNetworkingSend_Unreliable;
			case Delivery::UnreliableNoDelay: return k_nSteamNetworkingSend_UnreliableNoDelay;
			case Delivery::Reliable: break;
			}
			return k_nSteamNetworkingSend_Reliable;
		}

		void CountSent(const PacketTypeInfo& type, size_t bytes)
		{
			std::scoped_lock lock{ Data.StatsMutex };
			Data.Totals.PacketsSent++;
			Data.Totals.BytesSent += bytes;
			PacketTypeStats& stats = Data.TypeStats[type.Id];
			stats.Name = type.Name;
			stats.Sent++;
			stats.BytesSent += bytes;
		}

		void CountReceived(const PacketTypeInfo& type, size_t bytes)
		{
			std::scoped_lock lock{ Data.StatsMutex };
			Data.Totals.PacketsReceived++;
			Data.Totals.BytesReceived += bytes;
			PacketTypeStats& stats = Data.TypeStats[type.Id];
			stats.Name = type.Name;
			stats.Received++;
			stats.BytesReceived += bytes;
		}

		void CountDropped(const char* why, uint64_t sender)
		{
			{
				std::scoped_lock lock{ Data.StatsMutex };
				Data.Totals.PacketsDropped++;
			}
#if FLEX_DEBUG_TOOLS
			SDL_Log("Steam network: dropped a packet from %llu: %s", static_cast<unsigned long long>(sender), why);
#else
			(void)why;
			(void)sender;
#endif
		}

		void CountCounter(uint64_t NetworkStats::* counter)
		{
			std::scoped_lock lock{ Data.StatsMutex };
			++(Data.Totals.*counter);
		}

		// ---- Network thread ----------------------------------------------------------------------

		// Runs the packet's OnReceive here; queues it for the main thread when it asks for that.
		void Handle(PacketHandle packet, const PacketContext& context)
		{
			const PacketTypeInfo* type = packet.GetType();
			if (type->Receive(packet.Get(), context) == HandleResult::ToMainThread && type->ReceiveMain) {
				std::scoped_lock lock{ Data.InboxMutex };
				Data.Inbox.push_back(InboxPacket{ std::move(packet), context });
			}
		}

		void HandleLocally(PacketHandle packet, const PeerSnapshot& peers)
		{
			PacketContext context;
			context.Sender = SteamId{ peers.Self };
			context.bFromHost = peers.bIsHost;
			context.bIsLoopback = true;
			CountCounter(&NetworkStats::PacketsHandledLocally);
			Handle(std::move(packet), context);
		}

		class Batch
		{
		public:
			void Add(HSteamNetConnection connection, std::span<const std::byte> bytes, int flags, const PacketTypeInfo& type)
			{
				SteamNetworkingMessage_t* message = SteamNetworkingUtils()->AllocateMessage(static_cast<int>(bytes.size()));
				std::memcpy(message->m_pData, bytes.data(), bytes.size());
				message->m_conn = connection;
				message->m_nFlags = flags;
				Messages.push_back(message);
				if (std::find(Connections.begin(), Connections.end(), connection) == Connections.end()) {
					Connections.push_back(connection);
				}
				CountSent(type, bytes.size());
			}

			// One call for every message, then out at once (no waiting for Nagle: the thread batches already).
			void Send()
			{
				if (!Messages.empty()) {
					Sockets()->SendMessages(static_cast<int>(Messages.size()), Messages.data(), nullptr);
				}
				for (const HSteamNetConnection connection : Connections) {
					Sockets()->FlushMessagesOnConnection(connection);
				}
				Messages.clear();
				Connections.clear();
			}

		private:
			std::vector<SteamNetworkingMessage_t*> Messages;
			std::vector<HSteamNetConnection> Connections;
		};

		std::vector<std::byte> WriteHeader(Route route, uint64_t extra)
		{
			std::vector<std::byte> bytes;
			PacketWriter writer{ bytes };
			writer.Value(static_cast<uint8_t>(route));
			if (route == Route::RelayToUser || route == Route::Relayed) {
				writer.Value(extra);
			}
			return bytes;
		}

		void SendOne(OutgoingPacket& outgoing, const PeerSnapshot& peers, Batch& batch)
		{
			const PacketTypeInfo* type = outgoing.Packet.GetType();
			if (type->bIsStale) {
				return;
			}
			std::vector<HSteamNetConnection> destinations;
			Route route = Route::Direct;
			uint64_t extra = 0;
			bool bLocal = false;
			const uint64_t user = outgoing.To.User.Value;

			if (peers.bIsHost) {
				switch (outgoing.To.Type) {
				case Target::Kind::Host:
					bLocal = true;
					break;
				case Target::Kind::Everyone:
				case Target::Kind::Others:
					for (const auto& [id, connection] : peers.Clients) {
						destinations.push_back(connection);
					}
					bLocal = outgoing.To.Type == Target::Kind::Everyone;
					break;
				case Target::Kind::User:
					if (user == peers.Self) {
						bLocal = true;
					} else if (const auto found = peers.Clients.find(user); found != peers.Clients.end()) {
						destinations.push_back(found->second);
					}
					break;
				}
			} else if (peers.HostConnection != k_HSteamNetConnection_Invalid) {
				switch (outgoing.To.Type) {
				case Target::Kind::Host:
					destinations.push_back(peers.HostConnection);
					break;
				case Target::Kind::Everyone:
				case Target::Kind::Others:
					destinations.push_back(peers.HostConnection);
					route = Route::RelayToOthers;
					bLocal = outgoing.To.Type == Target::Kind::Everyone;
					break;
				case Target::Kind::User:
					if (user == peers.Self) {
						bLocal = true;
					} else {
						destinations.push_back(peers.HostConnection);
						if (user != peers.Host) {
							route = Route::RelayToUser;
							extra = user;
						}
					}
					break;
				}
			}

			if (!destinations.empty()) {
				std::vector<std::byte> bytes = WriteHeader(route, extra);
				EncodePacket(*type, outgoing.Packet.Get(), bytes);
				if (bytes.size() > static_cast<size_t>(k_cbMaxSteamNetworkingSocketsMessageSizeSend)) {
					SDL_Log("Steam network: %s is %zu bytes, more than Steam's limit; not sent.", type->Name.c_str(), bytes.size());
				} else {
					const int flags = ToSendFlags(outgoing.Mode);
					for (const HSteamNetConnection connection : destinations) {
						batch.Add(connection, bytes, flags, *type);
					}
				}
			}
			if (bLocal) {
				HandleLocally(std::move(outgoing.Packet), peers);
			}
		}

		bool IsAllowedFrom(const PacketTypeInfo& type, uint64_t origin, const PeerSnapshot& peers)
		{
			return type.SenderAuthority == Authority::Anyone || origin == peers.Host;
		}

		void Receive(std::span<const std::byte> bytes, uint64_t sender, int receivedFlags, const PeerSnapshot& peers, Batch& batch)
		{
			PacketReader reader{ bytes };
			uint8_t routeValue = 0;
			reader.Value(routeValue);
			const Route route = static_cast<Route>(routeValue);
			uint64_t extra = 0;
			if (route == Route::RelayToUser || route == Route::Relayed) {
				reader.Value(extra);
			}
			if (reader.IsFailed() || routeValue > static_cast<uint8_t>(Route::Relayed)) {
				CountDropped("bad header", sender);
				return;
			}
			const std::span<const std::byte> payload = bytes.subspan(bytes.size() - reader.GetRemaining());

			// Who wrote the packet: the sender, or for a relayed packet the client the host names.
			uint64_t origin = sender;
			if (peers.bIsHost) {
				if (route == Route::Relayed) {
					CountDropped("a client may not send relayed packets", sender);
					return;
				}
			} else {
				if (sender != peers.Host || route == Route::RelayToOthers || route == Route::RelayToUser) {
					CountDropped("not a route the host uses", sender);
					return;
				}
				if (route == Route::Relayed) {
					origin = extra;
				}
			}

			std::string error;
			PacketHandle packet = DecodePacket(payload, &error);
			if (!packet) {
				CountDropped(error.c_str(), sender);
				return;
			}
			const PacketTypeInfo& type = *packet.GetType();
			if (!IsAllowedFrom(type, origin, peers)) {
				CountDropped((type.Name + " may only come from the host").c_str(), sender);
				return;
			}
			CountReceived(type, bytes.size());

			// The host passes a client's packet on, with the client named as origin.
			if (peers.bIsHost && (route == Route::RelayToOthers || route == Route::RelayToUser)) {
				std::vector<std::byte> relayed = WriteHeader(Route::Relayed, sender);
				relayed.insert(relayed.end(), payload.begin(), payload.end());
				// Passed on as it came: reliable stays reliable.
				const int flags = (receivedFlags & k_nSteamNetworkingSend_Reliable) ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable;
				bool bForHost = route == Route::RelayToOthers;
				for (const auto& [id, connection] : peers.Clients) {
					const bool bWanted = route == Route::RelayToOthers ? id != sender : id == extra;
					if (bWanted) {
						batch.Add(connection, relayed, flags, type);
						CountCounter(&NetworkStats::PacketsRelayed);
					}
				}
				if (route == Route::RelayToUser && extra == peers.Self) {
					bForHost = true;
				}
				if (!bForHost) {
					return;
				}
			}

			PacketContext context;
			context.Sender = SteamId{ origin };
			context.bFromHost = origin == peers.Host;
			Handle(std::move(packet), context);
		}

		void ReceiveAll(const PeerSnapshot& peers, Batch& batch)
		{
			SteamNetworkingMessage_t* messages[ReceiveBatch];
			while (true) {
				int count = 0;
				if (peers.bIsHost) {
					count = peers.PollGroup != k_HSteamNetPollGroup_Invalid ? Sockets()->ReceiveMessagesOnPollGroup(peers.PollGroup, messages, ReceiveBatch) : 0;
				} else {
					count = peers.HostConnection != k_HSteamNetConnection_Invalid ? Sockets()->ReceiveMessagesOnConnection(peers.HostConnection, messages, ReceiveBatch) : 0;
				}
				if (count <= 0) {
					return;
				}
				for (int index = 0; index < count; ++index) {
					SteamNetworkingMessage_t* message = messages[index];
					if (const auto found = peers.ConnectionUsers.find(message->m_conn); found != peers.ConnectionUsers.end()) {
						Receive(std::span(static_cast<const std::byte*>(message->GetData()), message->GetSize()), found->second, message->m_nFlags, peers, batch);
					} else {
						CountDropped("unknown connection", 0);
					}
					message->Release();
				}
			}
		}

		void ThreadMain()
		{
			timeBeginPeriod(1); // so the 1 ms wait below is 1 ms, not the default 15.6 ms
			PeerSnapshot peers;
			uint32_t peersVersion = UINT32_MAX;
			std::vector<OutgoingPacket> sending;
			Batch batch;

			while (!Data.bStopThread) {
				{
					std::unique_lock lock{ Data.QueueMutex };
					Data.Wake.wait_for(lock, std::chrono::milliseconds(1), []() { return !Data.SendQueue.empty() || Data.bStopThread; });
					std::swap(sending, Data.SendQueue);
				}
				std::scoped_lock processing{ Data.ProcessingMutex };
				if (const uint32_t version = Data.PeersVersion; version != peersVersion) {
					std::scoped_lock lock{ Data.PeersMutex };
					peers = Data.Peers;
					peersVersion = version;
				}
				for (OutgoingPacket& outgoing : sending) {
					SendOne(outgoing, peers, batch);
				}
				sending.clear(); // destroys the packets here, under the processing lock
				ReceiveAll(peers, batch);
				batch.Send();
			}
			timeEndPeriod(1);
		}

		// ---- Main thread ---------------------------------------------------------------------------

		template<typename Function>
		void ChangePeers(Function&& change)
		{
			std::scoped_lock lock{ Data.PeersMutex };
			change(Data.Peers);
			Data.PeersVersion++;
		}

		void StartThread()
		{
			Data.bStopThread = false;
			Data.Thread = new std::thread(ThreadMain);
		}

		void StopThread()
		{
			if (!Data.Thread) {
				return;
			}
			Data.bStopThread = true;
			Data.Wake.notify_one();
			Data.Thread->join();
			delete Data.Thread;
			Data.Thread = nullptr;
		}

		void ForgetConnection(HSteamNetConnection connection)
		{
			ChangePeers([connection](PeerSnapshot& peers) {
				if (const auto found = peers.ConnectionUsers.find(connection); found != peers.ConnectionUsers.end()) {
					if (const auto client = peers.Clients.find(found->second); client != peers.Clients.end() && client->second == connection) {
						peers.Clients.erase(client);
					}
					peers.ConnectionUsers.erase(found);
				}
				if (peers.HostConnection == connection) {
					peers.HostConnection = k_HSteamNetConnection_Invalid;
				}
			});
			std::erase_if(Data.Pending, [connection](const PendingConnection& pending) { return pending.Connection == connection; });
		}

		void Close(HSteamNetConnection connection, const char* reason)
		{
			Sockets()->CloseConnection(connection, k_ESteamNetConnectionEnd_App_Min, reason, true);
			ForgetConnection(connection);
		}

		// One of the network's own connections (the process may have others).
		bool IsKnown(HSteamNetConnection connection)
		{
			{
				std::scoped_lock lock{ Data.PeersMutex };
				if (Data.Peers.ConnectionUsers.contains(connection)) {
					return true;
				}
			}
			return std::any_of(Data.Pending.begin(), Data.Pending.end(), [connection](const PendingConnection& pending) { return pending.Connection == connection; });
		}

		void ConnectionCallbacks::OnConnectionStatus(SteamNetConnectionStatusChangedCallback_t* status)
		{
			const HSteamNetConnection connection = status->m_hConn;
			const SteamNetConnectionInfo_t& info = status->m_info;
			const uint64_t user = info.m_identityRemote.GetSteamID64();
			if (info.m_eState != k_ESteamNetworkingConnectionState_Connecting && !IsKnown(connection)) {
				return;
			}

			switch (info.m_eState) {
			case k_ESteamNetworkingConnectionState_Connecting:
				// Someone connects to the host's listen socket: accepted in Update once they're a lobby member.
				if (Data.ListenSocket != k_HSteamListenSocket_Invalid && info.m_hListenSocket == Data.ListenSocket) {
					Data.Pending.push_back(PendingConnection{ connection, user, std::chrono::steady_clock::now() });
				}
				break;

			case k_ESteamNetworkingConnectionState_Connected: {
				bool bIsHostConnection = false;
				{
					std::scoped_lock lock{ Data.PeersMutex };
					bIsHostConnection = !Data.Peers.bIsHost && connection == Data.Peers.HostConnection;
				}
				if (bIsHostConnection) {
					Data.bHostConnected = true;
				}
				SDL_Log("Steam network: connected to %s.", Session::GetName(SteamId{ user }).c_str());
				break;
			}

			case k_ESteamNetworkingConnectionState_ClosedByPeer:
			case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
				SDL_Log("Steam network: connection to %s closed: %s", Session::GetName(SteamId{ user }).c_str(), info.m_szEndDebug);
				{
					std::scoped_lock lock{ Data.PeersMutex };
					if (connection == Data.Peers.HostConnection) {
						Data.bHostConnected = false;
					}
				}
				Sockets()->CloseConnection(connection, 0, nullptr, false);
				ForgetConnection(connection);
				break;

			default:
				break;
			}
		}

		bool Contains(const std::vector<SteamId>& users, uint64_t user)
		{
			return std::any_of(users.begin(), users.end(), [user](SteamId id) { return id.Value == user; });
		}

		// Host: accepts lobby members, refuses banned users and strangers.
		void ProcessPending()
		{
			if (Data.Pending.empty()) {
				return;
			}
			std::vector<SteamId> members;
			for (const LobbyMember& member : Lobby::GetMembers()) {
				members.push_back(member.Id);
			}
			const std::vector<SteamId> bans = Lobby::GetBans();
			const auto now = std::chrono::steady_clock::now();

			std::vector<PendingConnection> pending = std::move(Data.Pending);
			Data.Pending.clear();
			for (const PendingConnection& connection : pending) {
				if (Contains(bans, connection.User)) {
					Sockets()->CloseConnection(connection.Connection, k_ESteamNetConnectionEnd_App_Min, "banned", false);
				} else if (Contains(members, connection.User)) {
					if (Sockets()->AcceptConnection(connection.Connection) != k_EResultOK) {
						Sockets()->CloseConnection(connection.Connection, k_ESteamNetConnectionEnd_App_Min, "could not accept", false);
						continue;
					}
					Sockets()->SetConnectionPollGroup(connection.Connection, Data.Peers.PollGroup);
					HSteamNetConnection previous = k_HSteamNetConnection_Invalid;
					ChangePeers([&](PeerSnapshot& peers) {
						if (const auto found = peers.Clients.find(connection.User); found != peers.Clients.end()) {
							previous = found->second;
						}
						peers.Clients[connection.User] = connection.Connection;
						peers.ConnectionUsers[connection.Connection] = connection.User;
					});
					if (previous != k_HSteamNetConnection_Invalid) {
						Sockets()->CloseConnection(previous, k_ESteamNetConnectionEnd_App_Min, "reconnected", false);
						ChangePeers([previous](PeerSnapshot& peers) { peers.ConnectionUsers.erase(previous); });
					}
				} else if (now - connection.Since > PendingTimeout) {
					Sockets()->CloseConnection(connection.Connection, k_ESteamNetConnectionEnd_App_Min, "not a lobby member", false);
				} else {
					Data.Pending.push_back(connection);
				}
			}
		}

		// Before the game module unloads: nothing may still point into its code.
		void BeforeGameModuleUnload()
		{
			std::scoped_lock processing{ Data.ProcessingMutex };
			PacketRegistery::MarkStale([](const PacketTypeInfo& type) {
				return GameModule::IsInLoadedModule(reinterpret_cast<const void*>(type.Destroy));
			});
			const auto inModule = [](const PacketHandle& packet) {
				return GameModule::IsInLoadedModule(reinterpret_cast<const void*>(packet.GetType()->Destroy));
			};
			size_t dropped = 0;
			{
				std::scoped_lock lock{ Data.QueueMutex };
				dropped += std::erase_if(Data.SendQueue, [&](const OutgoingPacket& packet) { return inModule(packet.Packet); });
			}
			{
				std::scoped_lock lock{ Data.InboxMutex };
				dropped += std::erase_if(Data.Inbox, [&](const InboxPacket& packet) { return inModule(packet.Packet); });
			}
			if (dropped > 0) {
				SDL_Log("Steam network: dropped %zu queued packet(s) of the game module (hot reload).", dropped);
			}
		}
	}

	namespace Detail
	{
		void NetworkStartup()
		{
			SteamNetworkingUtils()->InitRelayNetworkAccess();
			Data.Callbacks = std::make_unique<ConnectionCallbacks>();
			if (Data.GameModuleListener == 0) {
				Data.GameModuleListener = GameModule::AddListener({ BeforeGameModuleUnload, {} });
			}
		}

		void NetworkShutdown()
		{
			NetworkStop();
			Data.Callbacks.reset();
		}

		void NetworkStart(bool bIsHost, SteamId self, SteamId host)
		{
			NetworkStop();
			{
				std::scoped_lock lock{ Data.StatsMutex };
				Data.Totals = {};
				Data.TypeStats.clear();
			}
			HSteamNetPollGroup pollGroup = k_HSteamNetPollGroup_Invalid;
			HSteamNetConnection hostConnection = k_HSteamNetConnection_Invalid;
			if (bIsHost) {
				Data.ListenSocket = Sockets()->CreateListenSocketP2P(0, 0, nullptr);
				pollGroup = Sockets()->CreatePollGroup();
			} else {
				SteamNetworkingIdentity identity;
				identity.SetSteamID64(host.Value);
				hostConnection = Sockets()->ConnectP2P(identity, 0, 0, nullptr);
			}
			ChangePeers([&](PeerSnapshot& peers) {
				peers = PeerSnapshot{};
				peers.bIsHost = bIsHost;
				peers.Self = self.Value;
				peers.Host = host.Value;
				peers.PollGroup = pollGroup;
				peers.HostConnection = hostConnection;
				if (hostConnection != k_HSteamNetConnection_Invalid) {
					peers.ConnectionUsers[hostConnection] = host.Value;
				}
			});
			Data.bHostConnected = false;
			Data.bActive = true;
			StartThread();
			SDL_Log("Steam network: started as %s.", bIsHost ? "host" : "client");
		}

		void NetworkStop()
		{
			if (!Data.bActive) {
				return;
			}
			StopThread();
			Data.bActive = false;
			Data.bHostConnected = false;

			PeerSnapshot peers;
			ChangePeers([&peers](PeerSnapshot& current) {
				peers = std::move(current);
				current = PeerSnapshot{};
			});
			for (const auto& [connection, user] : peers.ConnectionUsers) {
				Sockets()->CloseConnection(connection, k_ESteamNetConnectionEnd_App_Min, "left the lobby", true);
			}
			for (const PendingConnection& pending : Data.Pending) {
				Sockets()->CloseConnection(pending.Connection, k_ESteamNetConnectionEnd_App_Min, "left the lobby", false);
			}
			Data.Pending.clear();
			if (Data.ListenSocket != k_HSteamListenSocket_Invalid) {
				Sockets()->CloseListenSocket(Data.ListenSocket);
				Data.ListenSocket = k_HSteamListenSocket_Invalid;
			}
			if (peers.PollGroup != k_HSteamNetPollGroup_Invalid) {
				Sockets()->DestroyPollGroup(peers.PollGroup);
			}
			{
				std::scoped_lock lock{ Data.QueueMutex };
				Data.SendQueue.clear();
			}
			{
				std::scoped_lock lock{ Data.InboxMutex };
				Data.Inbox.clear();
			}
			SDL_Log("Steam network: stopped.");
		}

		void NetworkUpdate()
		{
			if (Data.bActive) {
				ProcessPending();
			}
		}

		void NetworkDisconnect(SteamId user, const char* reason)
		{
			HSteamNetConnection connection = k_HSteamNetConnection_Invalid;
			{
				std::scoped_lock lock{ Data.PeersMutex };
				if (const auto found = Data.Peers.Clients.find(user.Value); found != Data.Peers.Clients.end()) {
					connection = found->second;
				}
			}
			if (connection != k_HSteamNetConnection_Invalid) {
				Close(connection, reason);
			}
			for (const PendingConnection& pending : Data.Pending) {
				if (pending.User == user.Value) {
					Sockets()->CloseConnection(pending.Connection, k_ESteamNetConnectionEnd_App_Min, reason, false);
				}
			}
			std::erase_if(Data.Pending, [user](const PendingConnection& pending) { return pending.User == user.Value; });
		}

#if FLEX_DEBUG_TOOLS
		void NetworkTestAddClient(SteamId user, uint32_t connection)
		{
			Sockets()->SetConnectionPollGroup(connection, Data.Peers.PollGroup);
			ChangePeers([&](PeerSnapshot& peers) {
				peers.Clients[user.Value] = connection;
				peers.ConnectionUsers[connection] = user.Value;
			});
		}
#endif
	}

	bool Network::SendHandle(PacketHandle packet, Target target, Delivery delivery)
	{
		if (!Data.bActive || !packet) {
			return false;
		}
		const PacketTypeInfo& type = *packet.GetType();
		bool bIsHost = false;
		uint64_t self = 0;
		{
			std::scoped_lock lock{ Data.PeersMutex };
			bIsHost = Data.Peers.bIsHost;
			self = Data.Peers.Self;
		}
		if (type.SenderAuthority == Authority::HostOnly && !bIsHost) {
			SDL_Log("Steam network: %s is HostOnly; a client can't send it.", type.Name.c_str());
			return false;
		}
		if (target.Type == Target::Kind::User && target.User.Value != self) {
			const std::vector<LobbyMember> members = Lobby::GetMembers();
			if (std::none_of(members.begin(), members.end(), [&](const LobbyMember& member) { return member.Id == target.User; })) {
				return false;
			}
		}
		{
			std::scoped_lock lock{ Data.QueueMutex };
			Data.SendQueue.push_back(OutgoingPacket{ std::move(packet), target, delivery });
		}
		Data.Wake.notify_one();
		return true;
	}

	void Network::DispatchMainThread(World* world)
	{
		std::vector<InboxPacket> inbox;
		{
			std::scoped_lock lock{ Data.InboxMutex };
			std::swap(inbox, Data.Inbox);
		}
		if (!world) {
			return;
		}
		for (InboxPacket& packet : inbox) {
			const PacketTypeInfo* type = packet.Packet.GetType();
			if (!type->bIsStale && type->ReceiveMain) {
				type->ReceiveMain(packet.Packet.Get(), *world, packet.Context);
			}
		}
	}

	bool Network::IsActive()
	{
		return Data.bActive;
	}

	bool Network::IsConnected()
	{
		if (!Data.bActive) {
			return false;
		}
		std::scoped_lock lock{ Data.PeersMutex };
		return Data.Peers.bIsHost || Data.bHostConnected;
	}

	NetworkStats Network::GetStats()
	{
		std::scoped_lock lock{ Data.StatsMutex };
		NetworkStats stats = Data.Totals;
		for (const auto& [id, type] : Data.TypeStats) {
			stats.Types.push_back(type);
		}
		std::sort(stats.Types.begin(), stats.Types.end(), [](const PacketTypeStats& a, const PacketTypeStats& b) { return a.Name < b.Name; });
		return stats;
	}

	std::vector<ConnectionInfo> Network::GetConnections()
	{
		std::vector<ConnectionInfo> connections;
		if (!Data.bActive) {
			return connections;
		}
		std::vector<std::pair<HSteamNetConnection, uint64_t>> list;
		{
			std::scoped_lock lock{ Data.PeersMutex };
			for (const auto& [connection, user] : Data.Peers.ConnectionUsers) {
				list.emplace_back(connection, user);
			}
		}
		for (const auto& [connection, user] : list) {
			ConnectionInfo entry;
			entry.User = SteamId{ user };
			SteamNetConnectionRealTimeStatus_t status{};
			if (Sockets()->GetConnectionRealTimeStatus(connection, &status, 0, nullptr) == k_EResultOK) {
				entry.bIsConnected = status.m_eState == k_ESteamNetworkingConnectionState_Connected;
				entry.PingMilliseconds = status.m_nPing;
				entry.Quality = status.m_flConnectionQualityLocal;
				entry.PendingReliableBytes = static_cast<uint32_t>(status.m_cbPendingReliable);
				entry.PendingUnreliableBytes = static_cast<uint32_t>(status.m_cbPendingUnreliable);
			}
			connections.push_back(entry);
		}
		return connections;
	}
}
