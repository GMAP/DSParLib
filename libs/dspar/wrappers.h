#pragma once

#include <functional>
#include "dspar.h"
#include "SenderReceiver.h"
#include <type_traits>

namespace dspar
{

	struct Nothing
	{
		Nothing(void){};
		operator void(){}
	};

	std::ostream &operator<<(std::ostream &o, const Nothing &a)
	{
		return o << "Nothing { }";
	};

	class NothingSerializer : public SenderReceiver<Nothing>
	{
	public:
		void OnStart() override {}

		void Send(MPISender &sender, MessageHeader &msg, Nothing &nothing) override
		{
			throw std::runtime_error("Called send on NothingSerializer");
		};

		Nothing Receive(MPIReceiver &receiver, MessageHeader &msg) override
		{
			throw std::runtime_error("Called Receive on NothingSerializer");
		};
	};

	template <typename TIn, bool HasInput>
	class WrapperInputPolicy;

	template <typename TIn>
	class WrapperInputPolicy<TIn, true> {
	public:
		virtual void Process(TIn &data) = 0;
		virtual void OnFirstItem(TIn &data) { };
	};

	template <typename TIn>
	class WrapperInputPolicy<TIn, false> {
	public:
		void Process(Nothing &data) { };
		void OnFirstItem(Nothing &data) { };
	 };
	
	
	template <typename TOut, bool CanOutput>
	class WrapperOutputPolicy;


	template <typename TOut>
	class WrapperOutputPolicy<TOut, true> {
	public:
		void Emit(const TOut &data)
		{
			onEmit(data);
		}
	};

	template <typename TOut>
	class WrapperOutputPolicy<TOut, false> {
	};


	template <bool HasInput, bool CanOutput>
	class WrapperNeedsProducePolicy;

	template<> class WrapperNeedsProducePolicy<false, true> { //if the Wrapper has no input, but has output (like an emitter) then the user needs to implement produce themselves
	public:
		virtual void Produce() = 0;
	};

	template<> class WrapperNeedsProducePolicy<true, true> { //if the Wrapper has input and output (like a worker), implementation of Produce is prohibited
	public:
		void Produce() { };
	};
	
	template<> class WrapperNeedsProducePolicy<true, false> { //if the Wrapper has input and no output (like a collector), implementation of Produce is prohibited
	public:
		void Produce() { };
	};
	
	template<> class WrapperNeedsProducePolicy<false, false> { //if the Wrapper has no input and output, this is completely invalid usage. 
	//	virtual void Produce() { };
	};

#define NOT_NOTHING(t) !std::is_same<t, Nothing>::value && !std::is_same<t, void>::value
	template <typename TIn = Nothing, typename TOut = Nothing>
	class Wrapper:
		public WrapperInputPolicy<TIn, NOT_NOTHING(TIn)>, //HasInput = TIn != Nothing
		public WrapperOutputPolicy<TOut, NOT_NOTHING(TOut)>, //CanOutput = TOut != Nothing
		public WrapperNeedsProducePolicy<
			NOT_NOTHING(TIn), 
			NOT_NOTHING(TOut)>
	{
	private:
		std::function<void(const TOut &)> onEmit;

	public:
		virtual void Start() {};
		
		virtual void End() {}

		void SetEmitter(std::function<void(const TOut &)> emitter)
		{
			onEmit = emitter;
		}

		template <typename Out = TOut>
		typename std::enable_if<!std::is_same<Out, Nothing>::value, void>::type Emit(const Out &data)
		{
			onEmit(data);
		}
	};

	template <typename TIn>
	class InWrapper : public Wrapper<TIn, Nothing>
	{
	};

	template <typename TIn>
	class Collector : public InWrapper<TIn>
	{
	};

	template <typename TOut>
	class OutWrapper : public Wrapper<Nothing, TOut>
	{
	};

	template <typename TOut>
	class Emitter : public OutWrapper<TOut>
	{
	};

	template <typename TIn, typename TOut>
	class InOutWrapper : public Wrapper<TIn, TOut>
	{
	};

	template <typename TIn, typename TOut>
	class Worker : public Wrapper<TIn, TOut>
	{
	};

} // namespace dspar