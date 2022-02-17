#include <cassert>
#include <future>

#include <QGuiApplication>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QThread>
#include <QTimer>

#include "gui-client.hh"
#include "threaded-gui-factory.hh"
#include "abstract-gui-listener.hh"
#include "threaded-gui-proxy.hh"

namespace clap {

   std::weak_ptr<ThreadedGuiFactory> ThreadedGuiFactory::_instance;

   ThreadedGuiFactory::ThreadedGuiFactory() {
      std::promise<bool> initialized;
      _thread.reset(new std::thread([&initialized, this] {
         assert(!_app);
         assert(!QCoreApplication::instance());

         static int argc = 1;
         char arg0[] = "clap-plugin-gui";
         static char *argv[] = {arg0, nullptr};

         QGuiApplication::setAttribute(Qt::AA_PluginApplication);
         _app = std::make_unique<QGuiApplication>(argc, argv);

         _timer = std::make_unique<QTimer>();
         _timer->setInterval(1000 / 60);
         _timer->setTimerType(Qt::CoarseTimer);
         _timer->callOnTimeout([this] {
            onTimer();
         });
         _timer->start();

         initialized.set_value(true);

         _app->exec();
      }));

      if (!initialized.get_future().get()) {
         // failed to initialized
         _thread->join();
         _thread.reset();
      }
   }

   ThreadedGuiFactory::~ThreadedGuiFactory() {
      if (_app) {
         assert(_thread);
      }

      if (_thread) {
         assert(std::this_thread::get_id() != _thread->get_id());
         _thread->join();
         _thread.reset();
      }
   }

   std::shared_ptr<ThreadedGuiFactory> ThreadedGuiFactory::getInstance() {
      auto ptr = _instance.lock();
      if (ptr)
         return ptr;
      ptr.reset(new ThreadedGuiFactory());
      _instance = ptr;
      return ptr;
   }

   std::unique_ptr<GuiHandle>
   ThreadedGuiFactory::createGui(AbstractGuiListener &listener,
                                 const std::vector<std::string> &qmlImportPath) {
      assert(_app);
      assert(_thread);

      std::shared_ptr<GuiClient> ptr;
      QMetaObject::invokeMethod(
         _app.get(),
         [&] {
            assert(_app->thread() == QThread::currentThread());

            QStringList qtQmlImportPath;
            for (auto &s : qmlImportPath)
               qtQmlImportPath.append(QString::fromStdString(s));
            ptr = std::make_shared<GuiClient>(listener, qtQmlImportPath);

            if (ptr)
               _clients.emplace(&listener, ptr);
         },
         Qt::BlockingQueuedConnection);

      return std::make_unique<GuiHandle>(_instance.lock(), std::make_shared<ThreadedGuiProxy>(listener, ptr));
   }

   void ThreadedGuiFactory::releaseGui(GuiHandle &handle)
   {
      assert(_app);
      assert(_thread);

      auto g = dynamic_cast<GuiClient *>(&handle.gui());
      auto l = &g->guiListener();

      QMetaObject::invokeMethod(
         _app.get(),
         [&] {
            assert(_app->thread() == QThread::currentThread());

            _clients.erase(l);
         },
         Qt::BlockingQueuedConnection);
   }

   void ThreadedGuiFactory::onTimer()
   {
      assert(_app->thread() == QThread::currentThread());

      for (auto &it : _clients)
         it.first->onGuiPoll();
   }

} // namespace clap