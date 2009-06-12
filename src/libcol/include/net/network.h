#ifndef NETWORK_H
#define NETWORK_H

typedef struct ColNetwork
{
    int foo;
} ColNetwork;

ColNetwork *network_init();
ColStatus network_destroy(ColNetwork *net);

void network_start(ColNetwork *net);

#endif  /* NETWORK_H */
