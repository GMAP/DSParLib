#pragma once

#ifdef WINDOWS
#include <mpi.h>
#endif

#ifdef MACOSX
#include "mpi.h"
#endif

#ifndef MACOSX
#ifndef WINDOWS
#include "mpi.h"
#endif
#endif

#include <iostream>
#define COMMAND_SPAWN_NEW 1
#define COMMAND_SPAWN_ENDED 2

namespace dspar
{
	static bool finalizeScheduled = false;
	
	class MPIUtils
	{
	private:
		bool numberOfProcessesAlreadySet;
		
		MPI_Comm spawn(char **argv, MPI_Comm comm)
		{
			MPI_Comm intercomm;
			MPI_Comm_spawn(argv[0], &argv[1], 1, MPI_INFO_NULL, 0, comm, &intercomm, MPI_ERRCODES_IGNORE);
			return intercomm;
		}

	public:
		MPIUtils() : numberOfProcessesAlreadySet(false){};

		MPI_Comm SetTotalNumberOfProcesses(int argc, char **argv, int numberOfProcesses)
		{
			if (numberOfProcessesAlreadySet)
			{
				std::cerr << "Cannot set number of processes twice for now, only once" << std::endl;
			}

			MPI_Init(&argc, &argv);

			MPI_Comm parent;
			MPI_Comm_get_parent(&parent);

			if (parent == MPI_COMM_NULL)
			{
				MPI_Comm intracomm;
				MPI_Comm process = spawn(argv, MPI_COMM_WORLD);

				MPI_Intercomm_merge(process, 0, &intracomm);

				for (int i = 0; i < numberOfProcesses - 1; i++)
				{
					int commSize;
					MPI_Comm_size(intracomm, &commSize);

					for (int j = 1; j < commSize; j++)
					{
						int command = COMMAND_SPAWN_NEW;
						MPI_Send(&command, 1, MPI_INT, j, 0, intracomm);
					}
					MPI_Comm worker = spawn(argv, intracomm);
					MPI_Intercomm_merge(worker, 0, &intracomm);
				}

				{
					int commSize;
					MPI_Comm_size(intracomm, &commSize);
					for (int j = 1; j < commSize; j++)
					{
						int command = COMMAND_SPAWN_ENDED;
						MPI_Send(&command, 1, MPI_INT, j, 0, intracomm);
					}
				}

				MPI_Barrier(intracomm);
				numberOfProcessesAlreadySet = true;
				return intracomm;
			}
			else
			{
				MPI_Comm intracomm;

				MPI_Intercomm_merge(parent, 1, &intracomm);
				int myNewRank = -1;
				MPI_Comm_rank(intracomm, &myNewRank);

				int command = 0;
				MPI_Status status;

				while (true)
				{
					MPI_Recv(&command, 1, MPI_INT, 0, MPI_ANY_TAG, intracomm, &status);

					if (command == COMMAND_SPAWN_NEW)
					{
						MPI_Comm spawned = spawn(argv, intracomm);
						MPI_Intercomm_merge(spawned, 0, &intracomm);
						int myNewRank2 = -1;
						MPI_Comm_rank(intracomm, &myNewRank2);
					}
					else if (command == COMMAND_SPAWN_ENDED)
					{
						MPI_Comm_rank(intracomm, &myNewRank);
						break;
					}
				}

				MPI_Barrier(intracomm);
				numberOfProcessesAlreadySet = true;
				return intracomm;
			}
		}

		int GetMyRank(MPI_Comm &comm)
		{
			int rank = -1;
			MPI_Comm_rank(comm, &rank);
			return rank;
		}

		int GetCommSize(MPI_Comm &comm)
		{
			int size = -1;
			MPI_Comm_size(comm, &size);
			return size;
		}
	
		void Finalize() {
			MPI_Finalize();
		}

		void Init() {
			MPI_Init(&dspar::globals::argc, &dspar::globals::argv);
		}

		
		void Init(int* argc, char*** argv) {
			MPI_Init(argc, argv);
		}

		void ScheduleFinalizeAtProgramExit() {
			if (!finalizeScheduled) {
				std::atexit([]() {
					MPI_Finalize();
				});
				finalizeScheduled = true;
			}
		}

		void Barrier(MPI_Comm comm) {
			MPI_Barrier(comm);
		}
	};
} // namespace dspar