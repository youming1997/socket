# -*- coding: utf-8 -*-

from django.http import HttpResponse
from django.shortcuts import render
import sys

from TestModel.models import Test

reload(sys)
sys.setdefaultencoding("utf-8")


def testdb(request):
    # 添加数据
    # test1 = Test(name='朱指导')
    # test2 = Test(name='朱指导')
    # test1.save()
    # test2.save()
    # return HttpResponse("<p>数据添加成功</p>")

    # 获取数据
    # 初始化
    response = ""
    response1 = ""

    # 通过objects这个模型管理器的all()获得所有数据行，相当于SQL中的SELECT * FROM
    list = Test.objects.all()
    listlen = Test.objects.all().count()
    # filter相当于WHERE
    # response2 = Test.objects.filter(id=1)

    # 获取单个对象
    # response3 = Test.objects.get(id=1)

    # 限制返回的数据, 相当于OFFSET x LIMIT y
    # Test.objects.order_by('name')[0:2]

    # 输出所有数据
    response1 += """["""
    j = 1
    for var in list:
        response1 += """{\"id\": \"""" + str(var.id) + """\", \"name\": \"""" + str(var.name) + """\"}"""
        if j < listlen:
            response1 += """,\n"""
        else:
            response1 += """\n"""
        j += 1
    # response = response1
    response1 += """]"""
    print response1
    # return HttpResponse("<p>" + response + "</p>")
    return HttpResponse(response1)
