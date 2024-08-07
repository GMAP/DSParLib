#pragma once

#include "mpi.h"
#include <iostream>
namespace dspar
{
    template <typename T>
    struct AsyncMPIRequest
    {
        T data;
        MPI_Request request;
        void Await()
        {
            MPI_Status status;
            MPI_Wait(&request, &status);
        }
    };
} // namespace dspar