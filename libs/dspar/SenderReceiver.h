#pragma once

#include "dspar.h"

namespace dspar
{

	template <typename T>
	class SenderReceiver
	{
	private:
		bool started = false;

	public:
		virtual void Send(MPISender &sender, MessageHeader &msg, T &data) = 0;
		virtual T Receive(MPIReceiver &sender, MessageHeader &msg) = 0;
		void Start()
		{
			if (!started)
			{
				OnStart();
				started = true;
			}
		}

		virtual void OnStart(){};
	};

	template <typename T>
	class TrivialSendReceive : public SenderReceiver<T>
	{
	public:
		void Send(MPISender &sender, MessageHeader &msg, T &data)
		{
			sender.SendTo(msg, data);
		};
		T Receive(MPIReceiver &receiver, MessageHeader &msg)
		{
			T data;
			receiver.Receive(msg, &data);
			return data;
		};
	};

	template <typename T>
	TrivialSendReceive<T> SendReceive() {
		return TrivialSendReceive<T>();
	};

} // namespace dspar