vi ca.conf
vi server.conf
vi client.conf

openssl genrsa -out ca_private.key 4096
openssl req -new -x509 -days 730 -key ca_private.key -out ca_cert.crt -config ca.conf

export NEEDCHANGECODE=-aes256
export KEY_LEN=2048
openssl genrsa $NEEDCHANGECODE -out server_private_$KEY_LEN.key $KEY_LEN
openssl req -new -sha256 -out server_cert_$KEY_LEN.csr -key server_private_$KEY_LEN.key -config server.conf
openssl x509 -req -days 730 -CA ca_cert.crt -CAkey ca_private.key -CAcreateserial -in server_cert_$KEY_LEN.csr -out server_cert_$KEY_LEN.crt -extensions req_ext -extfile server.conf
cat server_cert_$KEY_LEN.crt server_private_$KEY_LEN.key > server_cert_$KEY_LEN.pem

openssl genrsa -out client_private.key 4096
openssl req -new -sha256 -out client_cert.csr -key client_private.key -config client.conf
openssl x509 -req -days 730 -CA ca_cert.crt -CAkey ca_private.key -in client_cert.csr -out client_cert.crt -extfile client.conf
openssl pkcs12 -export -clcerts -in client_cert.crt -inkey client_private.key -out client_cert.p12
password: 123


