---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/8/14 12:26 AM
---

-- need to make in ../../tsdb/native then source libPath.sh at first.
package.path = package.path .. ";../../?.lua"

local system = require("common.system")
local CfoxSQL = require("tsdb.foxSQL")
local fYaml = "fox.yaml"
local parser = CfoxSQL.new(fYaml)
local r
local correct

local sql = "SELECT * FROM tbl_abc WHERE name == 'zhaoyan'"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")

local SelectStmt = r.stmts[1].stmt.SelectStmt
assert( SelectStmt ~= nil, "SelectStmt parse error.")

local targetList = SelectStmt.targetList
assert( type(targetList) == "table", "targetList parse error.")
assert(type(targetList[1].ResTarget.val.ColumnRef.fields[1].A_Star) == "table", "parse select * failed.")

local fromClause = SelectStmt.fromClause
assert(fromClause[1].RangeVar.relname == "tbl_abc")


local whereClause = SelectStmt.whereClause
correct = parser:hasTimeLimit(whereClause)
if  correct then
    print(sql .. " is correct fox SQL")
else
    print(sql .. " is not correct fox SQL")
end
assert(whereClause.A_Expr.kind == "AEXPR_OP", "AEXPR_OP failed.")
assert(whereClause.A_Expr.name[1].String.str == "==", "AEXPR_OP string failed.")
assert(whereClause.A_Expr.lexpr.ColumnRef.fields[1].String.str == "name", "parser op lexpr failed.")
assert( whereClause.A_Expr.rexpr.A_Const.val.String.str == "zhaoyan", "parser op rexpr failed.")

assert(SelectStmt.limitOption == "LIMIT_OPTION_DEFAULT", "parser limitOption failed.")
assert(SelectStmt.op == "SETOP_NONE", "parser limitOption failed.")
print("parse sql \n\t" .. sql .. "\n ok.")

local sql = "SELECT name FROM tbl_abc WHERE age > 15"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")

local SelectStmt = r.stmts[1].stmt.SelectStmt
assert( SelectStmt ~= nil, "SelectStmt parse error.")

local targetList = SelectStmt.targetList
assert( type(targetList) == "table", "targetList parse error.")
assert(targetList[1].ResTarget.val.ColumnRef.fields[1].String.str == "name", "parse select name failed.")


local fromClause = SelectStmt.fromClause
assert(fromClause[1].RangeVar.relname == "tbl_abc")

local whereClause = SelectStmt.whereClause
assert(whereClause.A_Expr.kind == "AEXPR_OP", "AEXPR_OP failed.")
assert(whereClause.A_Expr.lexpr.ColumnRef.fields[1].String.str == "age", "parser op lexpr failed.")
assert(whereClause.A_Expr.name[1].String.str == ">", "parser op lexpr failed.")
assert( whereClause.A_Expr.rexpr.A_Const.val.Integer.ival == 15, "parser op rexpr failed.")

assert(SelectStmt.limitOption == "LIMIT_OPTION_DEFAULT", "parser limitOption failed.")
assert(SelectStmt.op == "SETOP_NONE", "parser limitOption failed.")
print("parse sql \n\t" .. sql .. "\n ok.")

local sql = "SELECT * FROM tbl_abc WHERE name NOT IN ('zhaoyan','qiaoke')"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")
print("parse sql \n\t" .. sql .. "\n ok.")

sql = "SHOW TABLES"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")
print("parse sql \n\t" .. sql .. "\n ok.")

sql = "SHOW DATABASES"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")
print("parse sql \n\t" .. sql .. "\n ok.")

sql = "SELECT SQLtime()"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")
print("parse sql \n\t" .. sql .. "\n ok.")

sql = "SELECT NOW() FROM tbl_abc"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")
print("parse sql \n\t" .. sql .. "\n ok.")

sql = "SELECT * FROM tbl_abc WHERE time > NOW(-10)"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")

local SelectStmt = r.stmts[1].stmt.SelectStmt
assert( SelectStmt ~= nil, "SelectStmt parse error.")

local targetList = SelectStmt.targetList
assert( type(targetList) == "table", "targetList parse error.")
assert(type(targetList[1].ResTarget.val.ColumnRef.fields[1].A_Star) == "table", "parse select * failed.")

local fromClause = SelectStmt.fromClause
assert(fromClause[1].RangeVar.relname == "tbl_abc")


local whereClause = SelectStmt.whereClause
--assert(whereClause.A_Expr.kind == "AEXPR_OP", "AEXPR_OP failed.")
--assert(whereClause.A_Expr.name[1].String.str == "==", "AEXPR_OP string failed.")
assert(whereClause.A_Expr.lexpr.ColumnRef.fields[1].String.str == "time", "parser op lexpr failed.")
--assert( whereClause.A_Expr.rexpr.A_Const.val.String.str == "zhaoyan", "parser op rexpr failed.")
correct = parser:hasTimeLimit(whereClause)
if  correct then
    print(sql .. " is correct fox SQL")
else
    print(sql .. " is not correct fox SQL")
end
print("parse sql \n\t" .. sql .. "\n ok.")

sql = "SELECT name, age FROM tbl_a,tbl_b WHERE age>15 and name IN ('zhaoyan','qiaoke')"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")
local SelectStmt = r.stmts[1].stmt.SelectStmt
local whereClause = SelectStmt.whereClause
correct = parser:hasTimeLimit(whereClause)
if  correct then
    print(sql .. " is correct fox SQL")
else
    print(sql .. " is not correct fox SQL")
end
print("parse sql \n\t" .. sql .. "\n ok.")

sql = "SELECT name, age FROM tbl_a,tbl_b WHERE time < NOW(-10)"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")
local SelectStmt = r.stmts[1].stmt.SelectStmt
local whereClause = SelectStmt.whereClause
correct = parser:hasTimeLimit(whereClause)
if  correct then
    print(sql .. " is correct fox SQL")
else
    print(sql .. " is not correct fox SQL")
end
print("parse sql \n\t" .. sql .. "\n ok.")

sql = "SELECT name, age FROM tbl_a,tbl_b WHERE time BETWEEN '1999-04-23 09:21:00' and '2000-04-23 09:21:00'"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")
local SelectStmt = r.stmts[1].stmt.SelectStmt
local whereClause = SelectStmt.whereClause
correct = parser:hasTimeLimit(whereClause)
if  correct then
    print(sql .. " is correct fox SQL")
else
    print(sql .. " is not correct fox SQL")
end
print("parse sql \n\t" .. sql .. "\n ok.")

sql = "SELECT name, age FROM tbl_a,tbl_b WHERE time > NOW(-10) and  age>15 and name IN ('zhaoyan','qiaoke')"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")

local SelectStmt = r.stmts[1].stmt.SelectStmt
local whereClause = SelectStmt.whereClause
correct = parser:hasTimeLimit(whereClause)
if  correct then
    print(sql .. " is correct fox SQL")
else
    print(sql .. " is not correct fox SQL")
end
assert(whereClause.BoolExpr ~= nil, "BoolExpr is nil.")
assert(whereClause.BoolExpr.args[1].A_Expr.lexpr.ColumnRef.fields[1].String.str == "time", "parser op lexpr failed.")
print("parse sql \n\t" .. sql .. "\n ok.")

sql = "SELECT name, age FROM tbl_a,tbl_b WHERE age>15 and time > NOW(-10) and name IN ('zhaoyan','qiaoke')"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")

local SelectStmt = r.stmts[1].stmt.SelectStmt
local whereClause = SelectStmt.whereClause
correct = parser:hasTimeLimit(whereClause)
if  correct then
    print(sql .. " is correct fox SQL")
else
    print(sql .. " is not correct fox SQL")
end
assert(whereClause.BoolExpr ~= nil, "BoolExpr is nil.")
assert(whereClause.BoolExpr.args[2].A_Expr.lexpr.ColumnRef.fields[1].String.str == "time", "parser op lexpr failed.")
print("parse sql \n\t" .. sql .. "\n ok.")

sql = "SELECT name, age FROM tbl_a,tbl_b WHERE time BETWEEN 1 and 2 and age>15 and name IN ('zhaoyan','qiaoke')"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")
local SelectStmt = r.stmts[1].stmt.SelectStmt
local whereClause = SelectStmt.whereClause
correct = parser:hasTimeLimit(whereClause)
if  correct then
    print(sql .. " is correct fox SQL")
else
    print(sql .. " is not correct fox SQL")
end
print("parse sql \n\t" .. sql .. "\n ok.")

sql = "SELECT * FROM tbl_abc WHERE time > DATE_SUB(NOW(),10)"
r = parser:parse(sql)
assert(r.error == nil, "bad res for " .. sql)
assert(r.cursorpos == nil, "bad res for " .. sql)
assert(r.version == 130008, "check pgsql failed.")

local SelectStmt = r.stmts[1].stmt.SelectStmt
assert( SelectStmt ~= nil, "SelectStmt parse error.")

local targetList = SelectStmt.targetList
assert( type(targetList) == "table", "targetList parse error.")
assert(type(targetList[1].ResTarget.val.ColumnRef.fields[1].A_Star) == "table", "parse select * failed.")

local fromClause = SelectStmt.fromClause
assert(fromClause[1].RangeVar.relname == "tbl_abc")


local whereClause = SelectStmt.whereClause
--assert(whereClause.A_Expr.kind == "AEXPR_OP", "AEXPR_OP failed.")
--assert(whereClause.A_Expr.name[1].String.str == "==", "AEXPR_OP string failed.")
assert(whereClause.A_Expr.lexpr.ColumnRef.fields[1].String.str == "time", "parser op lexpr failed.")
--assert( whereClause.A_Expr.rexpr.A_Const.val.String.str == "zhaoyan", "parser op rexpr failed.")
correct = parser:hasTimeLimit(whereClause)
if  correct then
    print(sql .. " is correct fox SQL")
else
    print(sql .. " is not correct fox SQL")
end
print("parse sql \n\t" .. sql .. "\n ok.")