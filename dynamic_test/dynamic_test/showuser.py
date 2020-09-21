#!/usr/bin/env python
# coding=utf-8

from django.shortcuts import render
import MySQLdb
import sys

reload(sys)
sys.setdefaultencoding('utf8')

def hello(request):
    conn = MySQLdb.connect(
        host='localhost',
        port=3306,
        user='root',
        passwd='',
        db='test',
        charset='utf8'
        )

    cur = conn.cursor()
    namelist_str = ""
    sql = "SELECT * from namelist"
    cur.execute(sql)

    name = cur.fetchall()
    # namelist_str += "["
    i = 1
    listLen = len(name)
    # for j in name:
    #     namelist_str += """{\"id\": \"""" + str(j[0]) + """\", \"name\": \"""" + str(j[1]) + """\"}"""
    #     if i < listLen:
    #         namelist_str += """,\n"""
    #     else:
    #         namelist_str += """\n"""
    for j in name:
        namelist_str += "<input value='" + str(j[0]) + "' readonly/>"
        namelist_str += "<input value='" + str(j[1]) + "'readonly/>"
        namelist_str += "<br/>"
        i += 1
    # namelist_str += "]"
    cur.close()
    conn.commit()
    conn.close()
    # context = {"name": namelist_str}
    return render(request, "test.html", {"name": namelist_str})
