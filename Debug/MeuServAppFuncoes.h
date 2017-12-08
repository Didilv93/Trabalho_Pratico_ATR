#include <boost/asio.hpp>
#include <iostream>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <mutex>
#include <thread>
#include <string>
#include <ctime>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/serialization/string.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>

using namespace boost::interprocess;
using boost::asio::ip::tcp;
using namespace std;
using namespace boost::archive;

struct position_t
{
	int id;
	time_t timestamp;
	double latitude;
	double longitude;
	int speed;
};

static const unsigned MAX_POSITION_SAMPLES = 30;
struct historical_data_request_t
{
	int id;
	int num_samples;
};
struct historical_data_reply_t
{
	int num_samples_available;
	position_t data[MAX_POSITION_SAMPLES];
};

struct active_users_t
{
	active_users_t() : num_active_users(0)
	{
		for (unsigned i = 0; i< LIST_SIZE; ++i)
		{
			list[i].id = -1;
		}
	}
	int num_active_users;
	enum { LIST_SIZE = 1000000 };
	position_t list[LIST_SIZE];
	boost::interprocess::interprocess_mutex mutex;
};