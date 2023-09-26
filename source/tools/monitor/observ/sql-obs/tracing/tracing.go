package tracing

import (
    "fmt"
)

func StartTracing() {
    fmt.Println("start tracing")
    StartTracingSql()
}