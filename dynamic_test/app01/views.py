# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.shortcuts import render, HttpResponse
from app01 import models
import sys

# Create your views here.
from app01.models import Book

reload(sys)
sys.setdefaultencoding("utf-8")


def index(request):
    return render(request, 'index.html')


def add_book(request):
    return render(request, 'addBook.html')


def add_author(request):
    return render(request, 'addAuthor.html')


def add_publish(request):
    return render(request, 'addPublish.html')


def add_book_author(request):
    return render(request, 'addBookAuthor.html')


def search_all_book(request):
    return render(request, 'searchAll.html')
