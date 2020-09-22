from django.http import HttpResponse
from django.shortcuts import render

def showuser(request):
    return render(request, 'showuser.html')