#if _WIN64
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <cstdlib>
#include <algorithm>
#include <random>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#if _WIN64
#define bassert(test) ( !!(test) ? (void)0 : ((void)printf("-- Assertion failed at line %d: %s\n", __LINE__, #test), __debugbreak()) )
#else
#define bassert assert
#endif

const int NOT_FOUND = -1;
const int NOT_SEARCHED = -2;

// Utility functions not needed for solution
namespace {

#include <stdint.h>

#if __linux__ != 0
#include <time.h>

static uint64_t timer_nsec() {
#if defined(CLOCK_MONOTONIC_RAW)
	const clockid_t clockid = CLOCK_MONOTONIC_RAW;

#else
	const clockid_t clockid = CLOCK_MONOTONIC;

#endif

	timespec t;
	clock_gettime(clockid, &t);

	return t.tv_sec * 1000000000UL + t.tv_nsec;
}

#elif _WIN64 != 0
#define NOMINMAX
#include <Windows.h>

static struct TimerBase {
	LARGE_INTEGER freq;
	TimerBase() {
		QueryPerformanceFrequency(&freq);
	}
} timerBase;

// the order of global initialisaitons is non-deterministic, do
// not use this routine in the ctors of globally-scoped objects
static uint64_t timer_nsec() {
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);

	return 1000000000ULL * t.QuadPart / timerBase.freq.QuadPart;
}

#elif __APPLE__ != 0
#include <mach/mach_time.h>

static struct TimerBase {
	mach_timebase_info_data_t tb;
	TimerBase() {
		mach_timebase_info(&tb);
	}
} timerBase;

// the order of global initialisaitons is non-deterministic, do
// not use this routine in the ctors of globally-scoped objects
static uint64_t timer_nsec() {
	const uint64_t t = mach_absolute_time();
	return t * timerBase.tb.numer / timerBase.tb.denom;
}

#endif

/// Allocate aligned for @count objects of type T, does not perform initialization
/// @param count - the number of objects
/// @param unaligned [out] - stores the un-aligned pointer, used to call free
/// @return pointer to the memory or nullptr
template <typename T>
T *alignedAlloc(size_t count, void *& unaligned) {
	const size_t bytes = count * sizeof(T);
	unaligned = malloc(bytes + 63);
	if (!unaligned) {
		return nullptr;
	}
	T* const aligned = reinterpret_cast<T*>(uintptr_t(unaligned) + 63 & -64);
	return aligned;
}

template <typename T>
struct AlignedArrayPtr {
	void *allocated = nullptr;
	T *aligned = nullptr;
	int64_t count = -1;

	AlignedArrayPtr() = default;

	AlignedArrayPtr(int64_t count) {
		init(count);
	}

	void init(int64_t newCount) {
		bassert(newCount > 0);
		free(allocated);
		aligned = alignedAlloc<T>(newCount, allocated);
		count = newCount;
	}

	void memset(int value) {
		::memset(aligned, value, sizeof(T) * count);
	}

	~AlignedArrayPtr() {
		free(allocated);
	}

	T *get() {
		return aligned;
	}

	const T *get() const {
		return aligned;
	}

	operator T *() {
		return aligned;
	}

	operator const T *() const {
		return aligned;
	}

	int64_t getCount() const {
		return count;
	}

	const T *begin() const {
		return aligned;
	}

	const T *end() const {
		return aligned + count;
	}

	int operator[](int index) const {
		return aligned[index];
	}

	int &operator[](int index) {
		return aligned[index];
	}

	AlignedArrayPtr(const AlignedArrayPtr &) = delete;
	AlignedArrayPtr &operator=(const AlignedArrayPtr &) = delete;
};

typedef AlignedArrayPtr<int> AlignedIntArray;

const char magic[] = ".BSEARCH";
const int magicSize = sizeof(magic) - 1;

bool storeToFile(const AlignedIntArray &hayStack, const AlignedIntArray &needles, const char *name) {
	FILE *file = fopen(name, "wb+");
	if (!file) {
		return false;
	}
	const char magic[] = ".BSEARCH";
	const int64_t sizes[2] = {hayStack.getCount(), needles.getCount()};

	fwrite(magic, 1, magicSize, file);
	fwrite(sizes, 1, sizeof(sizes), file);
	fwrite(hayStack.get(), sizeof(int), hayStack.getCount(), file);
	fwrite(needles.get(), sizeof(int), needles.getCount(), file);
	fclose(file);
	return true;
}

bool loadFromFile(AlignedIntArray &hayStack, AlignedIntArray &needles, const char *name) {
	FILE *file = fopen(name, "rb");
	if (!file) {
		return false;
	}
	
	char test[magicSize] = {0, };
	int64_t sizes[2];

	int allOk = true;
	allOk &= magicSize == fread(test, 1, magicSize, file);
	if (strncmp(magic, test, magicSize)) {
		printf("Bad magic constant in file [%s]\n", name);
		return false;
	}
	allOk &= sizeof(sizes) == fread(sizes, 1, sizeof(sizes), file);
	hayStack.init(sizes[0]);
	needles.init(sizes[1]);

	allOk &= hayStack.getCount() == fread(hayStack.get(), sizeof(int), hayStack.getCount(), file);
	allOk &= needles.getCount() == fread(needles.get(), sizeof(int), needles.getCount(), file);

	fclose(file);
	return allOk;
}

/// Verify if previous search produced correct results
/// @param hayStack - the input data that will be searched in
/// @param needles - the values that will be searched
/// @param indices - the indices of the needles (or -1 if the needle is not found)
/// Return the first index @c where find(@hayStack, @needles[@c]) != @indices[@c], or -1 if all indices are correct
int verify(const AlignedIntArray &hayStack, const AlignedIntArray & needles, const AlignedIntArray &indices) {
	for (int c = 0; c < needles.getCount(); c++) {
		const int value = needles[c];
		const int *pos = std::lower_bound(hayStack.begin(), hayStack.end(), value);
		const int idx = std::distance(hayStack.begin(), pos);

		if (idx == hayStack.getCount() || hayStack[idx] != value) {
			bassert(indices[c] == NOT_FOUND);
			if (indices[c] != NOT_FOUND) {
				return c;
			}
		} else {
			bassert(indices[c] == idx);
			if (indices[c] != idx) {
				return c;
			}
		}
	}
	return -1;
}

}

/// Stack allocator with predefined max size
/// The total memory is 64 byte aligned, all but the first allocation are not guaranteed to be algigned
/// Can only free all the allocations at once
struct StackAllocator {
	StackAllocator(uint8_t *ptr, int bytes)
		: totalBytes(bytes)
		, data(ptr) {}

	/// Allocate memory for @count T objects
	/// Does *NOT* call constructors
	/// @param count - the number of objects needed
	/// @return pointer to the allocated memory or nullptr
	template <typename T>
	T *alloc(int count) {
		const int size = count * sizeof(T);
		if (idx + size > totalBytes) {
			return nullptr;
		}
		uint8_t *start = data + idx;
		idx += size;
		return reinterpret_cast<T*>(start);
	}

	/// De-allocate all the memory previously allocated with @alloc
	void freeAll() {
		idx = 0;
	}

	/// Get the max number of bytes that can be allocated by the allocator
	int maxBytes() const {
		return totalBytes;
	}

	/// Get the free space that can still be allocated, same as maxBytes before any allocations
	int freeBytes() const {
		return totalBytes - idx;
	}

	StackAllocator(const StackAllocator &) = delete;
	StackAllocator& operator=(const StackAllocator &) = delete;
private:
	const int totalBytes;
	int idx = 0;
	uint8_t *data = nullptr;
	void *toFree = nullptr;
};

/// Binary search implemented to return same result as std::lower_bound
/// When there are multiple values of the searched, it will return index of the first one
/// When the searched value is not found, it will return -1
/// @param hayStack - the input data that will be searched in
/// @param needles - the values that will be searched
/// @param indices - the indices of the needles (or -1 if the needle is not found)

//O(mlogn) m is needles count and n - hayStack count
static void binarySearch(const AlignedIntArray &hayStack, const AlignedIntArray & needles, AlignedIntArray &indices) {
	for (int c = 0; c < needles.getCount(); c++) {
		const int value = needles[c];

		int left = 0;
		int count = hayStack.getCount();

		while (count > 0) {
			const int half = count / 2;

			if (hayStack[left + half] < value) {
				left = left + half + 1;
				count -= half + 1;
			} else {
				count = half;
			}
		}

		if (hayStack[left] == value) {
			indices[c] = left;
		} else {
			indices[c] = -1;
		}
        
	}
}
/// Implement some search algorithm and optimize it so it is faster than @binarySearch above
/// When the searched value is not found, it will return -1
/// @param hayStack - the input data that will be searched in
/// @param needles - the values that will be searched
/// @param indices - the indices of the needles (or -1 if the needle is not found)
/// @param allocator - all allocations must be done trough this allocator, it can be queried for max allowed allocation size
static void betterSearch(const AlignedIntArray &hayStack, const AlignedIntArray & needles, AlignedIntArray &indices, StackAllocator &allocator) {
	// TODO: Implement your solution here
    //populate a hash table 
    std::unordered_map<int64_t, int64_t> map;
    uint64_t curr_index = 1;
    uint64_t i;
    uint64_t hayStack_elements = hayStack.getCount();
    uint64_t needles_elements = needles.getCount();
    for (i = 0; i < hayStack_elements; i++)
    {
        map.insert(std::make_pair(hayStack[i], curr_index));
        curr_index++;
    }
    
    //loop through needles and populate the indices container
    for (i = 0; i < needles_elements; i++)
    {
        // -1 because if the element cannot be found the default return value is 0 so we just start from 1 instead of 0 and then subtract 1 on 
        // the value our map returns
        indices[i] = map[needles[i]] - 1;
    }
    
    //In total we have O(n) + O(m) or assymptotically O(max(n,m)) for time complexity -> n is hayStack elements count and m is number of needles;
    //Whereas binarySearch() has O(mlogn)
}

int main(int argc, char *argv[]) {
	printf("+ Correctness tests ... \n");

	const int heapSize = 1 << 13;
	const int64_t searches = 400ll * (1 << 26);

	// enumerate and run correctness test
	int testCaseCount = 0;
	for (int r = 0; /*no-op*/; r++) {
		AlignedArrayPtr<int> hayStack;
		AlignedArrayPtr<int> needles;
		char fname[64] = {0,};
		snprintf(fname, sizeof(fname), "%d.bsearch", r);


		if (!loadFromFile(hayStack, needles, fname)) {
			break;
		}

		printf("Checking %s... ", fname);

		AlignedArrayPtr<int> indices(needles.getCount());
		AlignedArrayPtr<uint8_t> heap(heapSize);

		StackAllocator allocator(heap, heapSize);
		{
			indices.memset(NOT_SEARCHED);
			betterSearch(hayStack, needles, indices, allocator);
			if (verify(hayStack, needles, indices) != -1) {
				printf("Failed to verify base betterSearch!\n");
				return -1;
			}

			indices.memset(NOT_SEARCHED);
			binarySearch(hayStack, needles, indices);
			if (verify(hayStack, needles, indices) != -1) {
				printf("Failed to verify base binarySearch!\n");
				return -1;
			}
		}
		printf("OK\n");
		++testCaseCount;
	}

	printf("+ Speed tests ... \n");

	for (int r = 0; r < testCaseCount; r++) {
		AlignedArrayPtr<int> hayStack;
		AlignedArrayPtr<int> needles;
		char fname[64] = {0,};
		snprintf(fname, sizeof(fname), "%d.bsearch", r);


		if (!loadFromFile(hayStack, needles, fname)) {
			printf("Failed to load %s for speed test, continuing\n", fname);
			continue;
		}

		const int testRepeat = std::min<int64_t>(1000ll, searches / hayStack.getCount());
		printf("Running speed test for %s, %d repeats \n", fname, testRepeat);

		AlignedArrayPtr<int> indices(needles.getCount());
		AlignedArrayPtr<uint8_t> heap(heapSize);

		StackAllocator allocator(heap, heapSize);
		uint64_t t0;
		uint64_t t1;

		// Time the binary search and take average of the runs
		{
			indices.memset(NOT_SEARCHED);
			t0 = timer_nsec();
			for (int test = 0; test < testRepeat; ++test) {
				binarySearch(hayStack, needles, indices);
			}
			t1 = timer_nsec();
		}

		const double totalBinary = (double(t1 - t0) * 1e-9) / testRepeat;
		printf("\tbinarySearch time %f\n", totalBinary);

		// Time the better search and take average of the runs
		{
			indices.memset(NOT_SEARCHED);
			t0 = timer_nsec();
			for (int test = 0; test < testRepeat; ++test) {
				betterSearch(hayStack, needles, indices, allocator);
			}
			t1 = timer_nsec();
		}

		const double totalBetter = (double(t1 - t0) * 1e-9) / testRepeat;
		printf("\tbetterSearch time %f\n", totalBetter);

		if (totalBetter < totalBinary) {
			printf("Great success!\n");
		}
	}
	return 0;
}