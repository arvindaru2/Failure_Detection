# MP2
## Justin Loew - jloew2
## Arvind Arunasalam - arunasa2

## Network Formats

### Backpropagation message format

typedef struct __attribute__((packed)) {
  time_t timestamp;
  uint8_t num_joined;
  node_id_t bytes[num_joined];
  uint8_t num_left;
  node_id_t bytes[num_left];
  uint8_t num_failed;
  node_id_t bytes[num_failed];
} net_bp_t;
