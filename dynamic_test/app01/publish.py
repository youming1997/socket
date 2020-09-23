# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.shortcuts import render, HttpResponse
from app01.models import Publish
import sys

# Create your views here.
from app01.models import Book

reload(sys)
sys.setdefaultencoding("utf-8")


def add_publish_message(request):
    if 'name' in request.GET and 'address' in request.GET and 'email' in request.GET:
        publish = Publish.objects.create(
            name=request.GET['name'],
            address=request.GET['address'],
            email=request.GET['email'], )
    return render(request, 'index.html', {"msg": "<p>添加出版社成功</p>"})


def search_all_publish(request):
    publishList = Publish.objects.all()
    listLen = Publish.objects.all().count()
    response = ""

    i = 1
    response += "["
    for var in publishList:
        response += "{\"id\": " + str(var.id) +\
                    ", \"name\": \"" + str(var.name) +\
                    "\"}"
        if i < listLen:
            response += ",\n"
        else:
            response += "\n"
        i += 1
    response += "]"

    return HttpResponse(response)