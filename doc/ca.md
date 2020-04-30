### Run CA env
```
docker run -it --name centos7 -v `pwd`/conf:/conf centos:centos7 /bin/sh

cd /conf/

cp /etc/pki/tls/openssl.cnf ./

rm -rf /etc/pki/CA/*.old

touch /etc/pki/CA/index.txt

echo 01 > /etc/pki/CA/serial

rm -rf certs;mkdir certs
```

### create base ca

```
openssl genrsa -out certs/root-ca.key 2048

openssl req -new -x509 -days 36500 -config ./openssl.cnf -key certs/root-ca.key -out certs/root-cacert.pem -subj "/C=CN/ST=Beijing/O=clibing/OU=clibing.com"

```

/C=CN/ST=Beijing/O=clibing/OU=`clibing.com`, the OU not CN, error from `https://www.cnblogs.com/emqx/p/11401278.html`

### check ca 

```
openssl x509 -in certs/root-cacert.pem -noout -text
```

### create server pem

```
openssl genrsa -out certs/server.key 2048

openssl req -new -days 3650 -key certs/server.key -out certs/server-cert.csr -subj "/C=CN/ST=Beijing/O=clibing/OU=clibing.com/CN=clibing.com"

openssl ca -config ./openssl.cnf -extensions v3_req -days 3650 -in certs/server-cert.csr -out certs/server-cert.pem -cert certs/root-cacert.pem -keyfile certs/root-ca.key
```

### verfiy

```
openssl verify -CAfile certs/root-cacert.pem certs/server-cert.pem
```


### client

```
openssl genrsa -out certs/client.key 2048

openssl req -new -days 3650 -key certs/client.key -out certs/client-cert.csr -subj "/C=CN/ST=Beijing/O=clibing/CN=clibing.com"

openssl ca -config ./openssl.cnf -extensions v3_req -days 3650 -in certs/client-cert.csr -out certs/client-cert.pem -cert certs/root-cacert.pem -keyfile certs/root-ca.key

```

### check look

```
openssl x509 -in certs/client-cert.pem -noout -text
```

### import

`O` `OU` `CN`， with request same
