#pragma once

#include "Message.h"
#include "DemandSignal.h"
#include "MPIUtils.h"
#include "AsyncMPIRequest.h"

namespace dspar
{

	//@TODO Fix static assert messages, some are incorrect
	template <typename T>
	class PreAllocator
	{
	private:
		T *memory_ptr;
		std::size_t memory_size;

	public:
		typedef std::size_t size_type;
		typedef T *pointer;
		typedef T value_type;

		PreAllocator(T *memory_ptr, std::size_t memory_size) : memory_ptr(memory_ptr), memory_size(memory_size) {}

		PreAllocator(const PreAllocator &other) throw() : memory_ptr(other.memory_ptr), memory_size(other.memory_size){};

		template <typename U>
		PreAllocator(const PreAllocator<U> &other) throw() : memory_ptr(other.memory_ptr), memory_size(other.memory_size){};

		template <typename U>
		PreAllocator &operator=(const PreAllocator<U> &other) { return *this; }
		PreAllocator<T> &operator=(const PreAllocator &other) { return *this; }
		~PreAllocator() {}

		pointer allocate(size_type n, const void *hint = 0) { return memory_ptr; }
		void deallocate(T *ptr, size_type n) {}

		size_type max_size() const { return memory_size; }
	};

	class MPIReceiver
	{
	private:
		MPI_Comm comm;

	public:
		MPIReceiver(MPI_Comm _comm) : comm(_comm) {}

		MessageHeader StartReceivingMessage()
		{
			MessageHeader header;
			MPI_Status status;
			MPI_Recv(&header, sizeof(MessageHeader), MPI_BYTE, MPI_ANY_SOURCE, MPI_DSPAR_MESSAGE_BOUNDARY, comm, &status);
			header.sender = status.MPI_SOURCE;
			return header;
		}

		AsyncMPIRequest<MessageHeader> StartReceivingMessageAsync()
		{
			AsyncMPIRequest<MessageHeader> asyncRequest;
			MPI_Irecv(&asyncRequest.data, sizeof(MessageHeader), MPI_BYTE, MPI_ANY_SOURCE, MPI_DSPAR_MESSAGE_BOUNDARY, comm, &asyncRequest.request);
			return asyncRequest;
		}

		DemandSignal StartReceivingDemand()
		{
			DemandSignal demand;
			MPI_Status status;
			MPI_Recv(&demand, sizeof(DemandSignal), MPI_BYTE, MPI_ANY_SOURCE, MPI_DSPAR_DEMAND, comm, &status);
			demand.sender = status.MPI_SOURCE;
			//LOG_DEBUG("Received demand from "<<status.MPI_SOURCE);
			return demand;
		}

		template <typename T>
		void Receive(MessageHeader &header, T *buffer)
		{
			MPI_Status status;
			MPI_Probe(header.sender, MPI_DSPAR_STREAM_MESSAGE, comm, &status);

			int count;
			MPI_Get_count(&status, MPI_BYTE, &count);

			if (count != sizeof(T))
			{
				SERDE_ERROR("Send and receives of wrong size. Got " << count << " bytes, expected to receive " << sizeof(T) << ". Aborting to prevent errors");
				MPI_Abort(comm, 1);
			}

			MPI_Recv(buffer, count, MPI_BYTE, header.sender, MPI_DSPAR_STREAM_MESSAGE, comm, &status);
		}

		template <typename T, size_t I, size_t J>
		void Receive(MessageHeader &header, T (*buffer)[I][J])
		{
			MPI_Status status;
			MPI_Probe(header.sender, MPI_DSPAR_STREAM_MESSAGE, comm, &status);

			int count;
			MPI_Get_count(&status, MPI_BYTE, &count);

			if (count != sizeof(T) * I * J)
			{
				SERDE_ERROR("Send and receives of wrong size. Got " << count << " byte, expected to receive " << sizeof(T) * I * J << ". Aborting to prevent errors");
				MPI_Abort(comm, 1);
			}

			MPI_Recv(buffer, count, MPI_BYTE, header.sender, MPI_DSPAR_STREAM_MESSAGE, comm, &status);
		}

		template <typename T, size_t I>
		void Receive(MessageHeader &header, T (*buffer)[I])
		{
			MPI_Status status;
			MPI_Probe(header.sender, MPI_DSPAR_STREAM_MESSAGE, comm, &status);

			int count;
			MPI_Get_count(&status, MPI_BYTE, &count);

			if (count != sizeof(T) * I)
			{
				SERDE_ERROR("Send and receives of wrong size. Got " << count << " byte, expected to receive " << sizeof(T) * I << ". Aborting to prevent errors");
				MPI_Abort(comm, 1);
			}

			MPI_Recv(buffer, count, MPI_BYTE, header.sender, MPI_DSPAR_STREAM_MESSAGE, comm, &status);
		}

		template <typename T, size_t I>
		void Receive(MessageHeader &header, std::array<T, I> *array)
		{

			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T** buffer to this function is forbidden - use Receive(header, buffer, dimension1, dimension2). Also check if you're not passing a double pointer (e.g. &arr where \"arr\" is T* already)");

			Receive(header, array->data(), I);
		}

		void Receive(MessageHeader &header, std::string *str)
		{
			size_t size;
			Receive(header, &size);
			if (size > 0)
			{
				str->resize(size);
				Receive(header, (char*)str->data(), size);
			}
		}

		template <typename T>
		void Receive(MessageHeader &header, std::vector<T> *array)
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T** buffer to this function is forbidden - use Receive(header, buffer, dimension1, dimension2). Also check if you're not passing a double pointer (e.g. &arr where \"arr\" is T* already)");

			size_t size;
			Receive(header, &size);
			if (size > 0)
			{
				array->resize(size);
				Receive(header, array->data(), size);
			}
		}
		/*
		template <typename T>
		void Receive(MessageHeader &header, std::vector<std::vector<T>> *array2d)
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T** buffer to this function is forbidden - use Receive(header, buffer, dimension1, dimension2). Also check if you're not passing a double pointer (e.g. &arr where \"arr\" is T* already)");
			
			size_t size;
			Receive(header, &size);
			if (size > 0)
			{
				array2d->reserve(size);
				for (int i = 0; i < size; i++)
				{
					size_t size;
					Receive(header, &size);
					std::vector<T> arr(size, T());
					Receive(header, arr.data(), size);
					array2d->push_back(arr);
				}
			}
		}
*/
		template <typename T>
		void Receive(MessageHeader &header, std::vector<std::vector<T>> *array2d)
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T** buffer to this function is forbidden - use Receive(header, buffer, dimension1, dimension2). Also check if you're not passing a double pointer (e.g. &arr where \"arr\" is T* already)");
			//Receive all sizes
			std::vector<size_t> sizes;
			Receive(header, &sizes);

			//Optimization: Resize to final size
			array2d->resize(sizes.size());

			//allocate arrays in 1 go
			for (int i = 0; i < sizes.size(); i++)
			{
				if (sizes[i] > 0)
				{
					(*array2d)[i].resize(sizes[i]);
				}
				else
				{
					//std::cout << "skipped at "<<i<<std::endl;
				}
			}

			//receive all data
			for (int i = 0; i < sizes.size(); i++)
			{
				if (sizes[i] > 0)
				{
					Receive(header, (*array2d)[i].data(), sizes[i]);
				}
				else
				{
					//std::cout << "skipped at "<<i<<std::endl;
				}
			}
		}

		template <typename T>
		void Receive(MessageHeader &header, std::vector<std::vector<std::vector<T>>> *array)
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T** buffer to this function is forbidden - use Receive(header, buffer, dimension1, dimension2). Also check if you're not passing a double pointer (e.g. &arr where \"arr\" is T* already)");
			size_t size;
			Receive(header, &size);
			if (size > 0)
			{
				array->reserve(size);
				for (int i = 0; i < size; i++)
				{
					std::vector<std::vector<T>> arr;
					Receive(header, &arr);
					array->push_back(arr);
				}
			}
		}

		template <typename T>
		void Receive(MessageHeader &header, T(*buffer), size_t dimension1)
		{

			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T** buffer to this function is forbidden - use Receive(header, buffer, dimension1, dimension2). Also check if you're not passing a double pointer (e.g. &arr where \"arr\" is T* already)");

			MPI_Status status;
			MPI_Probe(header.sender, MPI_DSPAR_STREAM_MESSAGE, comm, &status);

			SERDE_DEBUG("Sizeof " << typeid(T).name() << " is " << sizeof(T) << "and dimension1 is " << dimension1);

			int expectedCount = (int)(sizeof(T) * dimension1);

			int count;
			MPI_Get_count(&status, MPI_BYTE, &count);

			if (count != expectedCount)
			{
				SERDE_ERROR("Send and receives of wrong size. Got " << count << " bytes, expected to receive " << expectedCount << ". Aborting to prevent errors");
				MPI_Abort(comm, 1);
			}

			MPI_Recv(buffer, count, MPI_BYTE, header.sender, MPI_DSPAR_STREAM_MESSAGE, comm, &status);
		}

		template <typename T>
		void Receive(MessageHeader &header, T **buffer, size_t dimension1, size_t dimension2)
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "Wrong method call. Passing a T*** buffer to this function is forbidden - use Receive(header, buffer, dimension1, dimension2, dimension3). Also check if you're not passing a trple pointer (&arr where arr is T** already)");

			int expectedRowCount = (int)(sizeof(T) * dimension2);

			for (size_t i = 0; i < dimension1; i++)
			{
				MPI_Status status;
				MPI_Probe(header.sender, MPI_DSPAR_STREAM_MESSAGE, comm, &status);
				int count;
				MPI_Get_count(&status, MPI_BYTE, &count);
				if (count != expectedRowCount)
				{
					SERDE_ERROR("Send and receives of wrong size. Got " << count << " bytes, expected to receive " << expectedRowCount << ". Aborting to prevent errors now");
					MPI_Abort(comm, 1);
					break;
				}
				MPI_Recv(buffer[i], expectedRowCount, MPI_BYTE, header.sender, MPI_DSPAR_STREAM_MESSAGE, comm, &status);
			}
		}

		template <typename T>
		void Receive(MessageHeader &header, T ***buffer, size_t dimension1, size_t dimension2, size_t dimension3)
		{
			static_assert(std::is_trivial<T>::value,
						  "SendTo must be called with a type that is trivial (such as primitive types, structs). \nIf you need to send complex data structures, please send multiple messages.");

			static_assert(!std::is_pointer<T>::value,
						  "We only support passing dynamic arrays up to 3 levels of indirection (T***). If you need passing 4D arrays, send multiple T*** buffers instead. Also check if you're not passing a 4d pointer (&arr where arr is T*** already)");

			int expectedRowCount = sizeof(T) * dimension3;

			for (size_t i = 0; i < dimension1; i++)
			{
				for (size_t j = 0; j < dimension2; j++)
				{
					MPI_Status status;
					MPI_Probe(header.sender, MPI_DSPAR_STREAM_MESSAGE, comm, &status);
					int count;
					MPI_Get_count(&status, MPI_BYTE, &count);
					if (count != expectedRowCount)
					{
						SERDE_ERROR("Send and receives of wrong size. Got " << count << " bytes, expected to receive " << expectedRowCount << ". Aborting to prevent errors");
						MPI_Abort(comm, 1);
						break;
					}
					MPI_Recv(buffer[i][j], expectedRowCount, MPI_BYTE, header.sender, MPI_DSPAR_STREAM_MESSAGE, comm, &status);
				}
			}
		}

		MPI_Comm GetComm()
		{
			return comm;
		}
	};

} // namespace dspar
