package common

import (
    //"fmt"
    "database/sql"
    _ "github.com/go-sql-driver/mysql"
    "strconv"
    "strings"
)

type ConnDesc struct {
    dbDriverName string
    user         string
    passwd       string
    host         string
    port         int
    containerID  string
    podID        string
}

type DBConnect struct {
    db   *sql.DB
    desc *ConnDesc
}

type DataHandle func(rows *sql.Rows, data *[]string) error

func (dbConn *DBConnect) getDBDataByQueryCmd(
    query_cmd string, handler DataHandle, data *[]string) error {
    rows, err := dbConn.db.Query(query_cmd)
    if err != nil {
        return err
    }
    return handler(rows, data)
}

func getQueryRowInfo(rows *sql.Rows, data *[]string) error {
    columns, err := rows.Columns()
    if err != nil {
        return err
    }

    values := make([]interface{}, len(columns))
    for i := range values {
        values[i] = new(sql.RawBytes)
    }

    for rows.Next() {
        if err = rows.Scan(values...); err != nil {
            return err
        }
        rowData := ""
        for i, value := range values {
            //convert data to "key1=value1,key2=value1..."
            val := string(*value.(*sql.RawBytes))
            if len(val) == 0 {
                val = "NULL"
            }
            rowData += columns[i] + "=" + val + ","
        }
        if len(rowData) > 0 {
            (*data) = append(*data, rowData[0:(len(rowData)-1)])
        }
    }
    return rows.Err()
}

func getDataMapFromSliceData(data *[]string) []map[string]interface{} {
    if len(*data) <= 0 {
        return nil
    }
    mCnt := 0
    keyPos := 0
    mData := make([]map[string]interface{}, len(*data))
    s := strings.FieldsFunc((*data)[0], func(r rune) bool {
        return r == '=' || r == ','
    })
    //get index of real variable_name
    if strings.ToLower(s[0]) == "variable_name" &&
        strings.ToLower(s[2]) == "value" {
        keyPos = 1
    }
    //convert data "key1=value1,key2=value1..." to map['']=''
    for _, entry := range *data {
        if len(entry) <= 0 {
            continue
        }
        s := strings.FieldsFunc(entry, func(r rune) bool {
            return r == '=' || r == ','
        })
        sLen := len(s)
        m := make(map[string]interface{}, sLen/(2+2*keyPos))
        for i := keyPos; i < sLen; i += (2 + 2*keyPos) {
            m[s[i]] = s[i+1+keyPos]
        }
        mData[mCnt] = m
        mCnt++
    }
    return mData
}

/**
 * GetRowsByQueryCmd - get databese Row data by query cmd
 *
 * sql_cmd: query command
 *
 * if function successfully, a []map will return, and
 * The format of each element is 'key=value', the key is the
 * variable or columns name
 */
func (dbConn *DBConnect) GetRowsByQueryCmd(
    sql_cmd string) ([]map[string]interface{}, error) {
    var data []string
    err := dbConn.getDBDataByQueryCmd(
        sql_cmd, getQueryRowInfo, &data)
    if err != nil {
        PrintSysError(err)
        return nil, err
    }
    return getDataMapFromSliceData(&data), nil
}

func (dbConn *DBConnect) GetRowsListByQueryCmd(
    sql_cmd string) (*[]string, error) {
    var data []string
    err := dbConn.getDBDataByQueryCmd(
        sql_cmd, getQueryRowInfo, &data)
    if err != nil {
        PrintSysError(err)
        return nil, err
    }
    return &data, nil
}

func (dbConn *DBConnect) ParseSQL(sqlStr string) (string) {
    complexity := "simple"
    stmt, err := dbConn.db.Prepare(sqlStr)
    if err != nil {
        if strings.Contains(sqlStr, "JOIN") || strings.Contains(sqlStr, "GROUP") || 
           strings.Contains(sqlStr, "ORDER") || strings.Contains(sqlStr, "DISTINCT") ||
           strings.Contains(sqlStr, "UNION") {
            complexity = "complex"
        }
        return complexity
    }
    defer stmt.Close()

    keywords := make(map[string]bool)
    for _, word := range strings.Fields(strings.ToUpper(sqlStr)) {
        if _, ok := keywords[word]; !ok {
            keywords[word] = true
        }
    }

    if _, ok := keywords["JOIN"]; ok {
        complexity = "complex"
    }
    if _, ok := keywords["GROUP"]; ok {
        complexity = "complex"
    }
    if _, ok := keywords["ORDER"]; ok {
        complexity = "complex"
    }
    if _, ok := keywords["DISTINCT"]; ok {
        complexity = "complex"
    }
    if _, ok := keywords["LIMIT"]; ok {
        complexity = "complex"
    }
    if _, ok := keywords["UNION"]; ok {
        complexity = "complex"
    }
    return complexity
}

func (dbConn *DBConnect) GetPort() int {
    return dbConn.desc.port
}

func (dbConn *DBConnect) GetIp() string {
    return dbConn.desc.host
}

func (dbConn *DBConnect) GetContainerID() string {
    return dbConn.desc.containerID
}

func (dbConn *DBConnect) GetPodID() string {
    return dbConn.desc.podID
}

func (dbConn *DBConnect) DBConnIsVaild() bool {
    if dbConn.db != nil {
        return true
    }
    return false
}

func ConnectToDB(dbConn *DBConnect, user string, passwd string) {
    dataSourceName := user + ":" + passwd + "@tcp(" +
        dbConn.desc.host + ":" + strconv.Itoa(dbConn.desc.port) + ")/"
    db, err := sql.Open(dbConn.desc.dbDriverName, dataSourceName)
    if err != nil {
        PrintSysError(err)
        return
    }
    dbConn.db = db
}

func NewDBConnection(dbDriverName string,
    host string, port int, containerID string, podID string) (*DBConnect, error) {
    desc := ConnDesc{
        dbDriverName: dbDriverName,
        user:         "",
        passwd:       "",
        host:         host,
        port:         port,
        containerID:  containerID,
        podID:        podID,
    }
    dbConn := &DBConnect{
        db:   nil,
        desc: &desc,
    }
    return dbConn, nil
}

func (dbConn *DBConnect) CloseDBConnection() {
    if dbConn.DBConnIsVaild() {
        dbConn.db.Close()
    }
}
