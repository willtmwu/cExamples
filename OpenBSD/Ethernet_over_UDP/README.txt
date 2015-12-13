Description

The majority of the code and work are in the files if_eou.c and if_eou.h

For this project, a pseudo-device was created to route ethernet packets over the UDP protocol. Utilising the network interface API, socket callbacks had to be appropriately handled. 

The device must setup and establish connection with a server, hence only the client-side of the EOU protocol was developed. 

The connection had to be verified with a hash, and packets handled with mbufs. 

Changed Files in the kernel

sbin/ifconfig/ifconfig.c
sys/conf/GENERIC
sys/conf/files
sys/net/if_eou.c
sys/net/if_eou.h