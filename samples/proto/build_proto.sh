#!/bin/bash
protoc -I=./ --cpp_out=./code common.proto
