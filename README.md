# HomeNet

#### [early-stage implementation]

HomeNet is a zero-configuration TCP reverse proxy server that exposes all your local network servers to the public internet so that you can access all your local servers and services remotely. You don't have to tunnel each of your local servers separately.

## How to expose my local network using HomeNet

- Start a HomeNet instance on any supported device in your home. Read about how to install and run homenet in the `Usage` section.
- Once your homeNet bridge is up and running, you have to expose this instance to the internet, for this you have 3 options:
    - Setup port forwarding for the homenet port. For this, you need to make sure port forwarding works on your network.
    - Use Ngrok to tunnel your homenet port.
    - Use `Remote Listen` feature of HomeNet.
- Now each local server gets a unique URL to connect to. Your local server can be accessed either via its local IP address and port or via a listenId. Your URL will be in format: `hn://{Bridge URL}/{IP address:port or ListenId}`. Your bridge URL depends on how you have exposed it to the internet. 
    - If you are port-forwarding, it is your public IP address
    - If using ngrok, it is your ngrok URL
    - If using Remote Listen, it is basically `{Remote bridge URL}/{ListenID of your bridge}`.
- To connect to your new public URL, you can:
    - Use `connect` utility of the homeNet application, great for testing.
    - Use the `listen` utility of the application, this mode churns up a local TCP server on your computer and forwards all your messages to the HN URL you specify. This will be required for applications that are not homeNet compatible directly, for example existing ssh clients. To ssh into devices behind homeNet, start a `homenet l {HN URL}` and ssh to that local port.
    - Using HomeNet clients (Todo). I also plan to build an HTTP gateway server that will be able to forward HTTP requests to a server behind HomeNet bridge, to make it accessible on web browsers.
- You can configure your bridge to restrict/allow connections using local IP addresses, if it's a security issue for you, in that case, you can only connect using a listenId. You get a listenId when you are listening via Remote Listen or a listenId is assigned to you when you broadcast your service using mDNS. Applications supporting HomeNet can advertise their service on the local network using mDNS/Bonjour and the bridge recognizes it and assigns a listenId. You can query all such available services on your local network remotely and get their respective listenIds using the `query` utility of the application.

## Remote Listen for connections

Your HomeNet instance can connect to another HomeNet instance to listen for incoming connections, this is similar to Ngrok in theory. Use this feature when your homeNet instance is not accessible directly from the internet. Your instance starts a long-running TCP connection to the remote instance and gets notified when there is a new connection for it to handle. Your instance then starts another new connection to the remote to handle the incoming connection. This ultimately forms a chain for homeNet bridges. When you RL to a bridge, you receive a listenId that becomes a part of your public URL.

## Installing

- Download the source code from Github.
- `cd` to the downloaded folder.
- run `make`

```(bash)
cd /to/path/homenet
make
```

See usage in [docs](docs/usage).

## Why not Ngrok or other tunneling services

- Ngrok and others give you a `one-to-one` exposure where a single port on the ngrok server forwards messages to a particular port on your local machine. This has many practical drawbacks such as if you have multiple servers that you might want to access, you need to figure it out in advance and set up separate tunnels for each of them. 
- HomeNet provides `one-to-many` exposure where only a single port is used to expose all your localhost ports, this is done by sending some extra routing info during connection, more on that later.

## Why do I need a reverse proxy tunnel to access my local server

Tunneling servers like HomeNet and Ngrok solve a daunting problem with your local home network, if you ever happen to run a server on your personal device you will most probably notice that your friends cannot access it from their device (different network) when you give them your public IP and the port. One way to make your new server accessible is if you use wifi and your router supports Port Forwarding, you can configure your router to forward packets to your server. But your ISP provider should allow incoming outbound requests to your router, something that not all ISPs allow. Also, not everyone has access to the router admin panel. So if you are sitting in an office or a public cafe you will most probably not be able to port forward. This is where these services come in, they let you expose your local ports to the broader internet without any changes in your network environment.


## Usecases

The main utility of homeNet is to expose all your local services to the internet at once. This enables a lot of stuff:

### IoT devices

You might be having a lot of smart home/IoT devices that run a local server. You will be able to control all of them remotely without having to set up extra configurations for each of them.

### You have multiple multimedia servers running locally

If you use applications like `Plex` and `Kodi` on multiple devices like your pc or smart tv you can use homeNet to access your local media library when you are not at home.

### Decentralized clouds like nextCloud and NAS

Remotely access your self-hosted local nextCloud or File servers, NAS.

### SSH into any local device at home

You don't have to set up tunnels for each device that you might want to ssh into when you're not at home.

## Future work

HomeNet is in its early stage of development, though the application compiles runs as expected, you might encounter bugs. Please report them in the issues. Looking forward I would love to hear your inputs/suggestions on this project. Any help either in form of criticism, contributions, and suggestions is highly appreciated. Mainly these are the topics I need to work on next:

- Implementing client libraries and a gateway server for HTTP. The routing URL will be sent as an HTTP header in the request. This should support WebSockets too.
- Better documentation especially about the initial handshake and mDNS service.
- Better support for IPv6 in the code.
- Windows support.
- Use `union` type for structs like `hn_Socket` and `hn_Config`.
- Better logging.
- Support for daemonizing the program.
- Add encryption options using salt, since salt is preshared and never passed directly, we can use it for symmetric encryption.
- Add support for a holePunching option during initial handshake so that both sides can connect to each other directly. It would be using something like webRTC and basically act as a signaling server.
