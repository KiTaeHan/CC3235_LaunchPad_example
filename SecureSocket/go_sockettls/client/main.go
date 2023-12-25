package main

import (
	"bufio"
	"crypto/tls"
	"crypto/x509"
	"flag"
	"fmt"
	"os"
)

func main() {
	addr := flag.String("addr", "localhost:4040", "address to connet")
	serverCert := flag.String("scert", "server.crt", "server certificate")
	clientCert := flag.String("ccert", "client.crt", "client certificate")
	clientKey := flag.String("ckey", "client.key", "client key")
	flag.Parse()

	Scert, err := os.ReadFile(*serverCert)
	if err != nil {
		fmt.Println(err)
		return
	}
	ScertPoll := x509.NewCertPool()
	if ok := ScertPoll.AppendCertsFromPEM(Scert); !ok {
		fmt.Printf("unable to parse cert from %s", *serverCert)
		return
	}

	Ccert, err := tls.LoadX509KeyPair(*clientCert, *clientKey)
	if err != nil {
		fmt.Println(err)
		return
	}

	config := tls.Config{}

	// client`s certificate & key
	config.Certificates = []tls.Certificate{Ccert}
	// server`s certificate
	config.RootCAs = ScertPoll

	conn, err := tls.Dial("tcp", *addr, &config)
	if err != nil {
		fmt.Println(err)
		return
	}
	defer conn.Close()

	fmt.Fprintf(conn, "Hello, server\n")
	message, err := bufio.NewReader(conn).ReadString('\n')
	if err != nil {
		fmt.Println(err)
		return
	}
	fmt.Println(message)
}
