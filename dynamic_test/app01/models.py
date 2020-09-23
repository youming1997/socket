# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.db import models


# Create your models here.

class Book(models.Model):
    id = models.AutoField(primary_key=True)
    title = models.CharField(max_length=20)
    price = models.DecimalField(max_digits=5, decimal_places=2)
    publish = models.ForeignKey("Publish", on_delete=models.CASCADE)
    pub_date = models.DateField()
    authors = models.ManyToManyField("Author", null=True)


class Publish(models.Model):
    name = models.CharField(max_length=200)
    address = models.CharField(max_length=200)
    email = models.EmailField()


class Author(models.Model):
    name = models.CharField(max_length=20)
    age = models.SmallIntegerField(default=0)
    au_detail = models.ForeignKey("AuthorDetail", on_delete=models.CASCADE, unique=True)


class AuthorDetail(models.Model):
    gender_choices = (
        (0, "保密"),
        (1, "男"),
        (2, "女"),
    )
    gender = models.SmallIntegerField(choices=gender_choices)
    tel = models.CharField(max_length=20)
    address = models.CharField(max_length=200)
    birthday = models.DateField()