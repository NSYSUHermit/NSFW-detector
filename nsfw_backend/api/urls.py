from django.urls import path
from .views import SettingsView, HardwareControlView, StatusView, ApplicationsView

urlpatterns = [
    path('settings/', SettingsView.as_view(), name='settings'),
    path('hardware/<str:device>/', HardwareControlView.as_view(), name='hardware-control'),
    path('status/', StatusView.as_view(), name='status'),
    path('applications/', ApplicationsView.as_view(), name='applications'),
]