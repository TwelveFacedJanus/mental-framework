

P2PNETWORK_PORT: int = 31337
P2PNETWORK_MAGIC_WORD: str = "mental"
P2PNETWORK_VERSION: str = "1.0"
P2PNETWORK_MAX_PEER: int = 64


class MentalPeer:
    def __init__(self: object) -> None:
        self.ip: str = ""
        self.port: int = 0
    
    def set_port(self: object, port: int) -> None:
        self.port = port
    
    def set_ip(self: object, ip: str) -> None:
        self.ip = ip

class MentalServer:
    def __init__(self: object):
        self.peers: List[MentalPeer] = []
        self.server = websocket.serve(self.cmd, "localhost", P2PNETWORK_PORT)
    
    def cmd(self: object) ->
        package = websocket.recv()
        package_list: List[str] = package.split("\n")
        if package_list[0] == f"{P2PNETWORK_MAGIC_WORD} version {P2PNETWORK_VERSION}":
            self.peers.append(MentalPeer(ip=package_list[1], port=P2PNETWORK_PORT))
            