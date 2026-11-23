package main

type HardKV interface {
	Set(key string, value byte) Response
	Setex(key string, ttl uint32, value byte) Response
	Get(key string) Response
	Delete(key string) Response
	Evict(key string) Response
	Exist(key string) Response
	Expire(key string, ttl uint32) Response
	Ttl(key string) Response
	List(prefix string) Response
	Stat(metric string) Response
	Ping() Response
}

type Request struct {
	Version     uint16
	UsernameLen uint8
	Username    string
	Command     string
	Keylen      uint16
	Key         string
	ValueLen    uint32
	Value       []byte
}

type Response struct {
	Version  uint16
	Status   uint8
	MsgLen   uint8
	Message  string
	ValueLen uint32
	Value    []byte
}

type ResponseList struct {
	Version   uint16
	Status    uint8
	MsgLen    uint8
	Message   string
	Count     uint16
	EntryList []byte
}

type Client struct {
	address  string
	port     string
	secure   bool
	username string
}

func main() {
}
