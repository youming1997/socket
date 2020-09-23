# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.shortcuts import render, HttpResponse
from app01.models import Author, AuthorDetail
import sys

# Create your views here.
from app01.models import Book

reload(sys)
sys.setdefaultencoding("utf-8")


def add_author_message(request):
    if 'gender' in request.GET and 'tel' in request.GET and 'address' in request.GET and 'birthday' in request.GET:
        au_detail_id = AuthorDetail.objects.create(
            gender=request.GET['gender'],
            tel=request.GET['tel'],
            address=request.GET['address'],
            birthday=request.GET['birthday']).id  # type: int

    if 'name' in request.GET and 'age' in request.GET and au_detail_id:
        author = Author.objects.create(
            name=request.GET['name'],
            age=request.GET['age'],
            au_detail_id=au_detail_id)

    return render(request, 'index.html', {"msg": "<p>添加作者成功</p>"})


def search_all_author(request):
    authorList = Author.objects.all()
    listLen = Author.objects.all().count()
    response = ""

    i = 1
    response += "["
    for var in authorList:
        response += "{\"id\": " + str(var.id) + \
                    ", \"name\": \"" + str(var.name) + \
                    "\"}"
        if i < listLen:
            response += ",\n"
        else:
            response += "\n"
        i += 1
    response += "]"

    return HttpResponse(response)
