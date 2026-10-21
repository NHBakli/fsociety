#!/bin/sh
set -eu
umask 077
# MSYS2 doit conserver /CN=... sans le convertir en chemin Windows.
export MSYS2_ARG_CONV_EXCL='*'

# Une autorite locale de demonstration, jamais une cle partagee dans Git.
if [ -e certs ]; then
    echo 'certs existe deja : aucun certificat ne sera remplace.' >&2
    exit 1
fi
mkdir certs
openssl req -x509 -newkey rsa:2048 -nodes -sha256 -days 30 \
    -subj '/CN=Diagnostic Lab CA' \
    -addext 'basicConstraints=critical,CA:TRUE' \
    -addext 'keyUsage=critical,keyCertSign,cRLSign' \
    -keyout certs/ca.key -out certs/ca.crt 2>/dev/null

for role in server agent controller; do
    usage=clientAuth
    if [ "$role" = server ]; then usage=serverAuth; fi
    openssl req -new -newkey rsa:2048 -nodes -sha256 \
        -subj "/CN=$role.lab" -keyout "certs/$role.key" \
        -out "certs/$role.csr" 2>/dev/null
    cat > "certs/$role.ext" <<EOF
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=$usage
subjectAltName=DNS:$role.lab
EOF
    openssl x509 -req -sha256 -days 30 -in "certs/$role.csr" \
        -CA certs/ca.crt -CAkey certs/ca.key -CAcreateserial \
        -extfile "certs/$role.ext" -out "certs/$role.crt" 2>/dev/null
done
echo 'Certificats du laboratoire crees dans certs/ (validite : 30 jours).'
