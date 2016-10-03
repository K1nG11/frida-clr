#include "Device.hpp"

#include "Marshal.hpp"
#include "Process.hpp"
#include "Runtime.hpp"
#include "Session.hpp"

using System::Windows::Threading::DispatcherPriority;

namespace Frida
{
  static void OnDeviceLost (FridaDevice * device, gpointer user_data);

  Device::Device (FridaDevice * handle, Dispatcher ^ dispatcher)
    : handle (handle),
      dispatcher (dispatcher),
      icon (nullptr)
  {
    Runtime::Ref ();

    selfHandle = new msclr::gcroot<Device ^> (this);
    onLostHandler = gcnew EventHandler (this, &Device::OnLost);
    g_signal_connect (handle, "lost", G_CALLBACK (OnDeviceLost), selfHandle);
  }

  Device::~Device ()
  {
    if (handle == NULL)
      return;

    delete icon;
    icon = nullptr;
    g_signal_handlers_disconnect_by_func (handle, OnDeviceLost, selfHandle);
    delete selfHandle;
    selfHandle = NULL;

    this->!Device ();
  }

  Device::!Device ()
  {
    if (handle != NULL)
    {
      g_object_unref (handle);
      handle = NULL;

      Runtime::Unref ();
    }
  }

  String ^
  Device::Id::get ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Device");
    return Marshal::UTF8CStringToClrString (frida_device_get_id (handle));
  }

  String ^
  Device::Name::get ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Device");
    return Marshal::UTF8CStringToClrString (frida_device_get_name (handle));
  }

  ImageSource ^
  Device::Icon::get ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Device");
    if (icon == nullptr)
      icon = Marshal::FridaIconToImageSource (frida_device_get_icon (handle));
    return icon;
  }

  DeviceType
  Device::Type::get ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Device");

    switch (frida_device_get_dtype (handle))
    {
      case FRIDA_DEVICE_TYPE_LOCAL:
        return DeviceType::Local;
      case FRIDA_DEVICE_TYPE_TETHER:
        return DeviceType::Tether;
      case FRIDA_DEVICE_TYPE_REMOTE:
        return DeviceType::Remote;
      default:
        g_assert_not_reached ();
    }
  }

  array<Process ^> ^
  Device::EnumerateProcesses ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Device");

    GError * error = NULL;
    FridaProcessList * result = frida_device_enumerate_processes_sync (handle, &error);
    Marshal::ThrowGErrorIfSet (&error);

    gint result_length = frida_process_list_size (result);
    array<Process ^> ^ processes = gcnew array<Process ^> (result_length);
    for (gint i = 0; i != result_length; i++)
      processes[i] = gcnew Process (frida_process_list_get (result, i));

    g_object_unref (result);

    return processes;
  }

  unsigned int
  Device::Spawn (String ^ path, array<String ^> ^ argv, array<String ^> ^ envp)
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Device");

    gchar * pathUtf8 = Marshal::ClrStringToUTF8CString (path);
    gchar ** argvVector = Marshal::ClrStringArrayToUTF8CStringVector (argv);
    gchar ** envpVector = Marshal::ClrStringArrayToUTF8CStringVector (envp);
    GError * error = NULL;
    guint pid = frida_device_spawn_sync (handle, pathUtf8, argvVector, g_strv_length (argvVector), envpVector, g_strv_length (envpVector), &error);
    g_strfreev (envpVector);
    g_strfreev (argvVector);
    g_free (pathUtf8);
    Marshal::ThrowGErrorIfSet (&error);

    return pid;
  }

  void
  Device::Resume (unsigned int pid)
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Device");

    GError * error = NULL;
    frida_device_resume_sync (handle, pid, &error);
    Marshal::ThrowGErrorIfSet (&error);
  }

  Session ^
  Device::Attach (unsigned int pid)
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Device");

    GError * error = NULL;
    FridaSession * session = frida_device_attach_sync (handle, pid, &error);
    Marshal::ThrowGErrorIfSet (&error);

    return gcnew Session (session, dispatcher);
  }

  String ^
  Device::ToString ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Device");
    return String::Format ("Id: \"{0}\", Name: \"{1}\", Type: {2}", Id, Name, Type);
  }

  void
  Device::OnLost (Object ^ sender, EventArgs ^ e)
  {
    if (dispatcher->CheckAccess ())
      Lost (sender, e);
    else
      dispatcher->BeginInvoke (DispatcherPriority::Normal, onLostHandler, sender, e);
  }

  static void
  OnDeviceLost (FridaDevice * device, gpointer user_data)
  {
    (void) device;

    msclr::gcroot<Device ^> * wrapper = static_cast<msclr::gcroot<Device ^> *> (user_data);
    (*wrapper)->OnLost (*wrapper, EventArgs::Empty);
  }
}
