#include "net_types.hpp"

bool g18::isEqual(const node_id_t &lhs, const node_id_t &rhs)
{
  return (lhs.ip == rhs.ip) && (lhs.timestamp == rhs.timestamp);
}

bool g18::changelistIsEmpty(const changelist_t &lst)
{
  return lst.joined.empty() && lst.left.empty() && lst.failed.empty();
}
