# -*- coding: utf-8 -*-
from __future__ import unicode_literals

import json

from django.shortcuts import render, HttpResponse
from django.db.models import Q, F
from app01 import models
import sys

# Create your views here.

reload(sys)
sys.setdefaultencoding("utf-8")


def add_book_message(request):
    if 'title' in request.GET and 'price' in request.GET and 'pubdate' in request.GET and 'publish' in request.GET:
        book = models.Book.objects.create(
            title=request.GET['title'],
            price=request.GET['price'],
            pub_date=request.GET['pubdate'],
            publish_id=request.GET['publish'], )
        print book
        return render(request, 'index.html', {"msg": "<p>添加书籍成功</p>"})

    return render(request, 'addBook.html', {"msg": "<p>添加书籍失败</p>"})


def add_book_author_message(request):
    if 'book_id' in request.GET and 'author_id' in request.GET:
        book = models.Book.objects.get(id=request.GET['book_id'])
        authors = request.GET['author_id']
        print authors
        for i in authors:
            print i
            if i.isdigit():
                author = models.Author.objects.get(id=i)
                author.book_set.add(book)
        # book.authors.add(author)
        return HttpResponse("添加书籍作者成功")

    return render(request, 'addBookAuthor.html', {"msg": "<p>添加书籍作者失败</p>"})


def search_all_book(request):
    bookList = models.Book.objects.all()
    listLen = models.Book.objects.all().count()
    response = getlist(bookList, listLen)

    return HttpResponse(response)


def search_book_by_id(request):
    bookList = models.Book.objects.get(id=request.GET['message'])
    response = ""
    authors = ""
    i = 1
    authorList = bookList.authors.all()
    for author in authorList:
        authors += author.name
        if i < bookList.authors.all().count():
            authors += ","
        i += 1
    response += "["
    response += "{\"id\": " + str(bookList.id) + \
                ", \"title\": \"" + str(bookList.title) + \
                "\", \"price\": \"" + str(bookList.price) + \
                "\", \"pub_date\": \"" + str(bookList.pub_date) + \
                "\", \"publish\": \"" + str(bookList.publish.name) + \
                "\", \"author\": \"" + str(authors) + \
                "\"}"
    response += "]"

    return HttpResponse(response)


def search_book_by_title(request):
    bookList = models.Book.objects.filter(title=request.GET['message'])
    listLen = models.Book.objects.filter(title=request.GET['message']).count()
    response = getlist(bookList, listLen)

    return HttpResponse(response)


def search_book_by_price(request):
    bookList = models.Book.objects.filter(price=request.GET['message'])
    listLen = models.Book.objects.filter(price=request.GET['message']).count()
    response = getlist(bookList, listLen)

    return HttpResponse(response)


def search_book_by_publish(request):
    bookList = models.Book.objects.filter(publish__name=request.GET['message'])
    listLen = models.Book.objects.filter(publish__name=request.GET['message']).count()
    response = getlist(bookList, listLen)

    return HttpResponse(response)


def search_book_by_pubdate(request):
    bookList = models.Book.objects.filter(pub_date=request.GET['message'])
    listLen = models.Book.objects.filter(pub_date=request.GET['message']).count()
    response = getlist(bookList, listLen)

    return HttpResponse(response)


def getlist(booklist, listlen):
    response = ""

    i = 1
    j = 1
    response += "["
    for var in booklist:
        authors = ""
        authorList = var.authors.all()
        for author in authorList:
            authors += author.name
            if j < var.authors.all().count():
                authors += ","
            j += 1
        response += "{\"id\": " + str(var.id) + \
                    ", \"title\": \"" + str(var.title) + \
                    "\", \"price\": \"" + str(var.price) + \
                    "\", \"pub_date\": \"" + str(var.pub_date) + \
                    "\", \"publish\": \"" + str(var.publish.name) + \
                    "\", \"author\": \"" + str(authors) + \
                    "\"}"
        if i < listlen:
            response += ",\n"
        i += 1
    response += "]"

    print response
    return response
