#include <queue>
#include <mutex>
#include <condition_variable>
/**
 * Class representing a buffer with a fixed capacity.
 */
class BoundedBuffer {
  public:
	  // public constructor
	  BoundedBuffer(int max_size);
	  
	  // public member functions (a.k.a. methods)
	  int getItem();
	  void putItem(int new_item);

  private:
	  // private member variables (i.e. fields)
	  int capacity;
	  std::queue<int> buffer;
	  std::mutex thread_mutex;
	  std::condition_variable data_available;
	  std::condition_variable space_available;	
};
