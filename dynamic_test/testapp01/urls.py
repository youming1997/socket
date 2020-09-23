"""testapp01 URL Configuration

The `urlpatterns` list routes URLs to views. For more information please see:
    https://docs.djangoproject.com/en/1.11/topics/http/urls/
Examples:
Function views
    1. Add an import:  from my_app import views
    2. Add a URL to urlpatterns:  url(r'^$', views.home, name='home')
Class-based views
    1. Add an import:  from other_app.views import Home
    2. Add a URL to urlpatterns:  url(r'^$', Home.as_view(), name='home')
Including another URLconf
    1. Import the include() function: from django.conf.urls import url, include
    2. Add a URL to urlpatterns:  url(r'^blog/', include('blog.urls'))
"""
from django.conf.urls import url
from django.contrib import admin

from app01 import views, author, publish, book

urlpatterns = [
    url(r'^admin/', admin.site.urls),
    url('index/', views.index),
    url('addauthor/', views.add_author),
    url('addpublish/', views.add_publish),
    url('addbook/', views.add_book),
    url('addbookauthor/', views.add_book_author),

    url('addauthormessage/', author.add_author_message),
    url('addpublishmessage/', publish.add_publish_message),
    url('addbookmessage/', book.add_book_message),
    url('addbookauthormessage/', book.add_book_author_message),

    url('searchall/', views.search_all_book),
    url('searchallbook/', book.search_all_book),
    url('searchallpublish/', publish.search_all_publish),
    url('searchallauthor/', author.search_all_author),

    url('searchbookbyId/', book.search_book_by_id),
    url('searchbookbyTitle/', book.search_book_by_title),
    url('searchbookbyPrice/', book.search_book_by_price),
    url('searchbookbyPublish/', book.search_book_by_publish),
    url('searchbookbyPubdate/', book.search_book_by_pubdate),

    url('deletebook/', book.delete_book),
    url('updatebook/', views.update_book),
    url('updatebookmessage/', book.update_book),
]
