package main

import (
	"crypto/tls"
	"crypto/x509"
	"flag"
	"fmt"
	"net"
	"os"
)

func main() {
	port := flag.String("port", "4040", "listening port")
	serverCert := flag.String("scert", "server.crt", "server certificate")
	serverKey := flag.String("skey", "server.key", "server key")
	clientCert := flag.String("ccert", "client.crt", "client certificate")
	flag.Parse()

	Scert, err := tls.LoadX509KeyPair(*serverCert, *serverKey)
	if err != nil {
		fmt.Println(err)
		return
	}

	Ccert, err := os.ReadFile(*clientCert)
	if err != nil {
		fmt.Println(err)
		return
	}
	CcertPoll := x509.NewCertPool()
	if ok := CcertPoll.AppendCertsFromPEM(Ccert); !ok {
		fmt.Printf("unable to parse cert from %s", *clientCert)
		return
	}

	config := tls.Config{
		// requre Client's certificate
		ClientAuth: tls.RequireAndVerifyClientCert,
		ClientCAs:  CcertPoll,
		// server's certificate & key
		Certificates: []tls.Certificate{Scert},
	}

	fmt.Printf("listening on port %s\n", *port)
	l, err := tls.Listen("tcp", ":"+*port, &config)
	if err != nil {
		fmt.Println(err)
		return
	}
	defer l.Close()

	for {
		conn, err := l.Accept()
		if err != nil {
			fmt.Println(err)
			return
		}
		fmt.Printf("accepted connection from %s\n", conn.RemoteAddr())

		go handleConn(conn)
	}
}

func handleConn(conn net.Conn) {
	buf := make([]byte, 1024)
	_, err := conn.Read(buf)
	if err != nil {
		fmt.Println(err)
		return
	}

	fmt.Printf("Received message: %s", string(buf))
	res := fmt.Sprintf("Echo: %v", string(buf[:]))
	conn.Write([]byte(res))

	fmt.Printf("closing connection from %s\n", conn.RemoteAddr())
	conn.Close()
}
