rm go.mod
rm go.sum
go mod init metricSnappy.go
go mod tidy
go build -o libmetricSnappy.so -buildmode=c-shared metricSnappy.go
cp libmetricSnappy.so ../../beeQ/lib

