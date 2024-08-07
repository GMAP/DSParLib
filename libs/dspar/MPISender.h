#pragma once

#include "Message.h"
#include "DemandSignal.h"
#include "MPIUtils.h"
#include "AsyncMPIRequest.h"

namespace dspar
{
	//@TODO Fix static assert messages, some are incorrect
	class MPISender
	{
	private:
		MPI_Comm comm;
		int currentRank;

	public:
		uint64_t messagesSent;

		MPISender(MPI_Comm _comm) : comm(_comm), messagesSent(0)
		{
			dspar::MPIUtils utils;
			currentRank = utils.GetMyRank(_comm);
		}

		DemandSignal SendDemandSignalTo(int target, int amount)
		{
			DemandSignal msg;

			msg.target = target;
			msg.sender = currentRank;
			msg.amount = amount;
			MPI_Send(&msg, sizeof(DemandSignal), MPI_BYTE, target, MPI_DSPAR_DEMAND, comm);
			return msg;
		}

		AsyncMPIRequest<DemandSignal> SendDemandSignalToAsync(int target, int amount)
		{
			AsyncMPIRequest<DemandSignal> msg;

			msg.data.target = target;
			msg.data.sender = currentRank;
			msg.data.amount = amount;

			MPI_Isend(&msg.data, sizeof(DemandSignal), MPI_BYTE, target, MPI_DSPAR_DEMAND, comm, &msg.request);
			return msg;
		}

		template <typename T>
		AsyncMPIRequest<T> Await(AsyncMPIRequest<T> task)
		{
			MPI_Status status;
			MPI_Wait(&task.request, &status);
			return task;
		}
#ifdef DSPARTIMINGS
		MessageHeader StartSendingMessageTo(int target,
											uint64_t id = std::numeric_limits<uint64_t>::max(),
											Duration::rep prev = 0,
											Timings timings = Timings{0, 0, 0})
		{
			MessageHeader msg;
			if (id == std::numeric_limits<uint64_t>::max())
			{
				msg.id = messagesSent++;
			}
			else
			{
				msg.id = id;
			}
			msg.target = target;
			msg.sender = currentRank;
			msg.type = MESSAGE_TYPE;

			msg.totalComputeTime = timings.totalComputeTime;

			if (prev == 0)
			{
				msg.ts = Clock::now().time_since_epoch().count();
			}
			else
			{
				msg.ts = prev;
			}

			MPI_Send(&msg, sizeof(msg), MPI_BYTE, target, MPI_DSPAR_MESSAGE_BOUNDARY, comm);

			return msg;
		}
#else
		MessageHeader StartSendingMessageTo(int target, uint64_t id = std::numeric_limits<uint64_t>::max())
		{
			MessageHeader msg;
			if (id == std::numeric_limits<uint64_t>::max())
			{
				msg.id = messagesSent++;
			}
			else
			{
				msg.id = id;
			}
			msg.target = target;
			msg.sender = currentRank;
			msg.type = MESSAGE_TYPE;

			MPI_Send(&msg, sizeof(msg), MPI_BYTE, target, MPI_DSPAR_MESSAGE_BOUNDARY, comm);

			return msg;
		}
#endif

		MessageHeader SendStopMessageTo(int target, uint64_t id = std::numeric_limits<uint64_t>::max())
		{
			MessageHeader msg;
			if (id == std::numeric_limits<uint64_t>::max())
			{
				msg.id = messagesSent++;
			}
			else
			{
				msg.id = id;
			}
			msg.target = target;
			msg.sender = currentRank;
			msg.type = STOP_TYPE;

			MPI_Send(&msg, sizeof(msg), MPI_BYTE, target, MPI_DSPAR_MESSAGE_BOUNDARY, comm);
			return msg;
		}

		template <typename T>
		void SendTo(const MessageHeader &header, T *buffer, size_t dimension)
		{
			SERDE_DEBUG("SendTo(T*) Sending data with sizeof T = " << GetTypeSize<T>() << " and dimension = " << dimension);

			size_t dataSizeInBytes = GetTypeSize<T>();
			size_t totalBytes = dataSizeInBytes * dimension;

			MPI_Send(buffer, (int)totalBytes, MPI_BYTE, header.target, MPI_DSPAR_STREAM_MESSAGE, comm);
		}

		template <typename T>
		void SendTo(const MessageHeader &header, T **buffer, size_t dimension1, size_t dimension2)
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T*** buffer to this function is forbidden - use SendTo(target, buffer, dimension1, dimension2, dimension3)");

			SERDE_DEBUG("SendTo(T**) Sending data with sizeof T = " << GetTypeSize<T>());
			size_t dataSizeInBytes = GetTypeSize<T>();
			size_t totalRowBytes = dataSizeInBytes * dimension2;

			for (size_t i = 0; i < dimension1; i++)
			{
				MPI_Send(buffer[i], (int)totalRowBytes, MPI_BYTE, header.target, MPI_DSPAR_STREAM_MESSAGE, comm);
			}
		}

		template <typename T>
		void SendTo(const MessageHeader &header, T ***buffer, size_t dimension1, size_t dimension2, size_t dimension3)
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T**** buffer to this function is forbidden - use SendTo(target, buffer, dimension1, dimension2, dimension3)");

			SERDE_DEBUG("SendTo(T**) Sending data with sizeof T = " << GetTypeSize<T>());
			size_t dataSizeInBytes = GetTypeSize<T>();
			size_t totalRowBytes = dataSizeInBytes * dimension3;

			for (size_t i = 0; i < dimension3; i++)
			{
				for (size_t j = 0; j < dimension2; i++)
				{
					MPI_Send(buffer[i][j], totalRowBytes, MPI_BYTE, header.target, MPI_DSPAR_STREAM_MESSAGE, comm);
				}
			}
		}

		template <typename T, size_t I>
		void SendTo(const MessageHeader &header, T (&buffer)[I])
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T* buffer to this function is forbidden - use SendTo(target, buffer, dimension1)");

			SERDE_DEBUG("SendTo(T[" << I << "]) Sending data with sizeof T = " << GetTypeSize<T>() << " and sizeof(buffer) = " << sizeof(buffer));

			size_t totalBytes = sizeof(buffer);

			MPI_Send(buffer, (int)totalBytes, MPI_BYTE, header.target, MPI_DSPAR_STREAM_MESSAGE, comm);
		}

		template <typename T, size_t I, size_t J>
		void SendTo(const MessageHeader &header, T (&buffer)[I][J])
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing any pointer of T to this function is forbidden - use SendTo(target, buffer, dimension1, dimension2)");

			SERDE_DEBUG("SendTo(T[" << I << "][" << J << "]) Sending data with sizeof T = " << GetTypeSize<T>() << " and sizeof(buffer) = " << sizeof(buffer));

			size_t totalBytes = sizeof(buffer);

			MPI_Send(buffer, (int)totalBytes, MPI_BYTE, header.target, MPI_DSPAR_STREAM_MESSAGE, comm);
		}

		template <typename T>
		void SendTo(const MessageHeader &header, T &buffer)
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T* buffer to this function is forbidden - use SendTo(target, buffer, dimension1)");

			SERDE_DEBUG("SendTo(T) Sending data with sizeof T = " << GetTypeSize<T>() << " and sizeof(buffer) = " << sizeof(buffer));

			size_t totalBytes = sizeof(T);
			MPI_Send(&buffer, (int)totalBytes, MPI_BYTE, header.target, MPI_DSPAR_STREAM_MESSAGE, comm);
		}

		template <typename T, size_t I>
		void SendTo(const MessageHeader &header, std::array<T, I> &array)
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T* buffer to this function is forbidden - use SendTo(target, buffer, dimension1)");

			SERDE_DEBUG("SendTo(T) Sending std::array... Calling SendTo(T*, size_t size)");

			SendTo(header, array.data(), I);
		}

		void SendTo(const MessageHeader &header, std::string &str)
		{
			size_t totalBytes = str.length();
			SendTo(header, totalBytes);
			if (totalBytes > 0)
			{
				SendTo(header, (char*)str.data(), totalBytes);
			}
		}

		template <typename T>
		void SendTo(const MessageHeader &header, std::vector<T> &vector)
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T* buffer to this function is forbidden");

			SERDE_DEBUG("SendTo(T) Sending std::vector... Calling SendTo(T*, size_t size)");
			size_t size = vector.size();
			SendTo(header, size);
			if (vector.size() > 0)
			{
				SendTo(header, vector.data(), size);
			}
		}

		/*template <typename T>
		void SendTo(const MessageHeader &header, std::vector<std::vector<T>> &vectors)
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T* buffer to this function is forbidden");
			size_t size = vectors.size();
			SendTo(header, size);
			for (auto& vec: vectors) {
			    SendTo(header, vec);			
			}
		}*/

		template <typename T>
		void SendTo(const MessageHeader &header, std::vector<std::vector<T>> &vectors)
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T* buffer to this function is forbidden");

			std::vector<size_t> sizes;
			for (auto &vec : vectors)
			{
				sizes.push_back(vec.size());
			}
			SendTo(header, sizes);

			for (auto &vec : vectors)
			{
				if (vec.size() > 0)
				{
					SendTo(header, vec.data(), vec.size());
				}
			}
		}

		template <typename T>
		void SendTo(const MessageHeader &header, std::vector<std::vector<std::vector<T>>> &vec3d)
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T* buffer to this function is forbidden");

			SendTo(header, vec3d.size());
			for (auto &vec2d : vec3d)
			{
				SendTo(header, vec2d);
			}
		}

		template <typename T>
		size_t GetTypeSize()
		{
			static_assert(std::is_trivial<T>::value,
						  "GetTypeSize must be called with a type that is trivial (such as primitive types, structs).");

			static_assert(!std::is_pointer<T>::value,
						  "Dont call this method with a pointer type directly");

			return sizeof(T);
		}

		MPI_Comm GetComm()
		{
			return comm;
		}
	};
} // namespace dspar