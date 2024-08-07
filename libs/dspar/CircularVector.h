#pragma once

#include <algorithm>
#include <vector>

namespace dspar
{

	template <typename T>
	class CircularVector
	{

	private:
		std::vector<T> vector;
		size_t currentIndex;

	public:
		CircularVector(std::vector<T> initialVector) : vector(initialVector), currentIndex(0){};
		CircularVector() : vector(std::vector<int>()), currentIndex(0){};

		T Next()
		{

			T result = vector[currentIndex];

			if (currentIndex == vector.size() - 1)
			{
				currentIndex = 0;
			}
			else
			{
				currentIndex++;
			}
			return result;
		}

		void Add(const T item)
		{
			vector.push_back(item);
		}

		void Remove(const T item)
		{
			auto begin = vector.begin();
			auto end = vector.end();
			vector.erase(std::remove(begin, end, item), end);
		}

		const size_t Count()
		{
			return vector.size();
		}

		const size_t IsEmpty()
		{
			return Count() == 0;
		}

		std::vector<T> Data()
		{
			return vector;
		}
	};
} // namespace dspar