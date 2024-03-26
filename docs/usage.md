
# Usage

There are 5 modes homeNet can run as. `Bridge` being the default and most important one. You can configure homeNet either via a config file, cli arguments, or env variables.

### Bridge

Bridge mode listens for incoming TCP connections, locally and/or remotely (via RL), and forwards messages to their proper destination.
When a new connection is established, the client sends a `CONNECT {url}` message and the bridge creates another connection to the destination URL and starts relaying all messages to each other after this. The bridge also keeps track of local services advertised over mDNS and assigns them a unique temporary listenId. It also allows other clients to remote listen through the bridge.

Start without RL on port 2000:
```(bash)
./homenet b -p 2000
```

Start with RL with remote bridge url:
```(bash)
./homenet b -rl true -url "hngate.herokuapp.com"
```

To listen to a static listenId you need the salt associated with it. Allowed key/salt pairs can be set in the config file.
This will make `cherry01` its listenId:
```(bash)
./homenet b -rl true -url "hngate.herokuapp.com" -key cherry01 -salt a123
```

Start without mDNS service:
```(bash)
./homenet b -mdns false
```

### Listen

Listen mode starts a local TCP server, listens for incoming connections, and relays all messages to the particular URL it is configured with.
No extra routing info is required, unlike the bridge mode.

```(bash)
./homenet l -p 2000 -url "hngate.herokuapp.com/cherry01/192.168.0.23:80"
```

Listen on a random port:
```(bash)
./homenet l -url "192.168.0.23:80"
```

### Remote Listen

Remote Listen mode listens for incoming connections via a remote bridge (RL feature of bridge) and forwards all connections to a particular local server as configured.

```(bash)
./homenet rl -url "hngate.herokuapp.com" -ip "192.168.0.23:80"
```

### Connect

Connect mode creates a connection to the given URL, sends the data you provide, and prints all data received. It handles all homeNet related handshakes for you.

```(bash)
./homenet c -url "hngate.herokuapp.com/cherry01/192.168.0.23:80" -data "hello!"
```

If you don't provide data as an argument, it asks you for input once the connection is established.

### Query

Query mode connects to a bridge URL and queries for available local services and prints their info including their listenIds.

```(bash)
./homenet q -url "hngate.herokuapp.com/cherry01" -name "photos.hn.local"
```

If authentication is required by the target bridge, you need to provide a valid query key/salt using `-key` and `-salt` params.

