package analyzer

import (
    // "encoding/json"
    // "fmt"
    // "os"
    "reflect"
    "strconv"
)

type OsCheckConfig struct {
    Window uint32 `json:"window" default:"6"`
    // Window uint32 `json:"window" default:"10"`
    Iowait uint32 `json:"iowait" default:"5"`
    Await  uint32 `json:"await" default:"10"`
    Util   uint32 `json:"util" default:"20"`
    Iops   uint32 `json:"iops" default:"150"`
    Bps    uint32 `json:"bps" default:"30720"` // KB/s
}

func setOsCheckConfig(config *OsCheckConfig, path string) {
    // content, err := os.ReadFile("config.json")
    // if err != nil {
    //     fmt.Println("config file not exist.")
    //     // return
    // }
    // if err := json.Unmarshal(content, config); err != nil {
    //     panic(err)
    // }
    val := reflect.ValueOf(config).Elem()
    for i := 0; i < val.NumField(); i++ {
        parseField(val.Field(i), val.Type().Field(i))
    }
}

func parseField(value reflect.Value, field reflect.StructField) error {
    tagVal := field.Tag.Get("default")

    if !value.IsZero() {
        // A value is set on this field so there's no need to set a default value.
        return nil
    }

    switch value.Kind() {
    case reflect.Uint32:
        i, err := strconv.ParseUint(tagVal, 10, 32)
        if err != nil {
            return err
        }
        value.SetUint(i)
        return nil
    }
    return nil
}
