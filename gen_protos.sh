#!/bin/bash

rm -rf bot/protocol/{connection,game}
mkdir -p bot/protocol/{connection,game}

protoc --cpp_out=bot/protocol/connection --proto_path=proto/connection login_message.proto

for p in proto/game/*.proto; do
    protoc --cpp_out=bot/protocol/game --proto_path=proto/game $(basename $p)
done
