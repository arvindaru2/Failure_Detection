#include <iostream>
#include <cerrno>
#include <fstream>
#include <cstring>
#include <sstream>
#include <vector>
#include <list>
#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>

#include <cstdlib>
#include <cstdio>



typedef uint16_t lamp_time_t;

typedef uint32_t persistent_node_id_t; // The node's IP address

typedef struct __attribute__((packed)) {
  persistent_node_id_t ip;
  lamp_time_t timestamp;
} node_id_t;

bool isEqual(const node_id_t lhs, const node_id_t rhs)
{
  return (lhs.ip == rhs.ip) && (lhs.timestamp == rhs.timestamp);
}

/////////////////////////////////////////
// Network Types
/////////////////////////////////////////

#define FORWARD_PROP_PORT 31337
#define BACK_PROP_PORT 31338

// A listing of all changes that we have not yet seem come full circle around the ring. For local use only; not in network format.
typedef struct {
  std::list<node_id_t> joined, left, failed;
  lamp_time_t timestamp;
} changelist_t;

bool changelistIsEmpty(const changelist_t &lst)
{
  return lst.joined.empty() && lst.left.empty() && lst.failed.empty();
}

std::string generateCLPacket(changelist_t theChanges)
{
	std::string packet;
	std::stringstream theStream;
	theStream << theChanges.timestamp;
	int j, l, f;
	j = theChanges.joined.size();
	l = theChanges.left.size();
	f = theChanges.failed.size();
	node_id_t val;
	theStream << "j";
	for(int j1 = 0; j1 < j; j1++)
	{
		val = theChanges.joined.front();
		if(val.ip == 10)
		{
			theStream << 10;
		} 
		else
		{
			theStream << 0;
			theStream << val.ip;
		}
		theStream << val.timestamp;
		theStream << "m";
		theChanges.joined.pop_front();		
	}
	theStream << "l";
	for(int l1 = 0; l1 < l; l1++)
	{
		val = theChanges.left.front();
		if(val.ip == 10)
		{
			theStream << 10;
		} 
		else
		{
			theStream << 0;
			theStream << val.ip;
		}
		theStream << val.timestamp;
		theStream << "m";
		theChanges.left.pop_front();	
	}
	theStream << "f";
	for(int f1 = 0; f1 < f; f1++)
	{
		val = theChanges.failed.front();
		if(val.ip == 10)
		{
			theStream << 10;
		} 
		else
		{
			theStream << 0;
			theStream << val.ip;
		}
		theStream << val.timestamp;
		theStream << "m";
		theChanges.failed.pop_front();	
	}
	packet = theStream.str();
	return packet;
}



changelist_t generateCL(std::string CLPacket)
{

	std::string temp;
	changelist_t ret;
	node_id_t hold;
	int j;
	int k;
	int val;
	int val2;
	for(int i = 0; CLPacket[i] != 'j'; i++)
	{
		temp += CLPacket[i];
		j = i;
	}
	std::istringstream iss (temp);
	iss >> val;
	ret.timestamp = val;
	j = j+2;

	while(CLPacket[j] != 'l')
	{
		temp = "";
		k = 0;
		for(int i = j; CLPacket[i] != 'm'; i++)
		{
			temp += CLPacket[i];
			j = i;
			k++;
			if(k ==2)
			{
				temp += " ";
			}
		}
		std::istringstream iss (temp);
		iss >> val;
		iss >> val2;
		hold.ip = val;
		hold.timestamp = val2;
		ret.joined.push_back(hold);

		j = j+2;				
	}
	
	j++;

	while(CLPacket[j] != 'f')
	{
		temp = "";
		k = 0;
		for(int i = j; CLPacket[i] != 'm'; i++)
		{
			temp += CLPacket[i];
			j = i;
			k++;
			if(k ==2)
			{
				temp += " ";
			}
		}
		std::istringstream iss (temp);
		iss >> val;
		iss >> val2;
		hold.ip = val;
		hold.timestamp = val2;
		ret.left.push_back(hold);

		j = j+2;				
	}

	j++;

	while(j < CLPacket.size())
	{
		temp = "";
		k = 0;
		for(int i = j; CLPacket[i] != 'm'; i++)
		{
			temp += CLPacket[i];
			j = i;
			k++;
			if(k ==2)
			{
				temp += " ";
			}
		}
		std::istringstream iss (temp);
		iss >> val;
		iss >> val2;
		hold.ip = val;
		hold.timestamp = val2;
		ret.failed.push_back(hold);

		j = j+2;				
	}

	return ret;
}






int main()
{

	changelist_t theChanges;

	theChanges.timestamp = 5;
	node_id_t test1;
	test1.timestamp = 55543;
	test1.ip = 2;
	node_id_t test2;
	test2.timestamp = 54321;
	test2.ip = 4;
	std::cout<< "begin the log formation" << "\n";
	theChanges.joined.push_back(test1);
	theChanges.joined.push_back(test2);
	theChanges.joined.push_back(test2);

	theChanges.left.push_back(test1);
	theChanges.left.push_back(test2);

	theChanges.failed.push_back(test2);
	theChanges.failed.push_back(test2);
	theChanges.failed.push_back(test1);

	std::cout<< "created the log" << "\n";	

	std::string result = generateCLPacket(theChanges);
	std::cout<< result << "\n";

	changelist_t newChange = generateCL(result);
	result = generateCLPacket(newChange);
	std::cout<< result << "\n";


}

