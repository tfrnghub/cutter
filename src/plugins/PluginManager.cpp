
#ifdef CUTTER_ENABLE_PYTHON
#include <Python.h>
#endif

#include "PluginManager.h"
#include "PythonManager.h"
#include "CutterPlugin.h"
#include "CutterPythonPlugin.h"

#include <QDir>
#include <QCoreApplication>
#include <QPluginLoader>
#include <QStandardPaths>

Q_GLOBAL_STATIC(PluginManager, uniqueInstance)

PluginManager *PluginManager::getInstance()
{
    return uniqueInstance;
}

PluginManager::PluginManager()
{
}

PluginManager::~PluginManager()
{
}

void PluginManager::loadPlugins()
{
    assert(plugins.isEmpty());

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    if (locations.isEmpty()) {
        qCritical() << "Failed to get a standard path to load plugins from.";
        return;
    }
    QDir pluginsDir(locations.first());

    pluginsDir.mkdir("plugins");
    if (!pluginsDir.cd("plugins")) {
        return;
    }

    QDir nativePluginsDir = pluginsDir;
    nativePluginsDir.mkdir("native");
    if (nativePluginsDir.cd("native")) {
        loadNativePlugins(nativePluginsDir);
    }

    QDir pythonPluginsDir = pluginsDir;
    pythonPluginsDir.mkdir("python");
    if (pythonPluginsDir.cd("python")) {
        loadPythonPlugins(pythonPluginsDir.absolutePath());
    }

    qInfo() << "Loaded" << plugins.length() << "plugin(s).";
}


void PluginManager::destroyPlugins()
{
    for (CutterPlugin *plugin : plugins) {
        delete plugin;
    }
}

void PluginManager::loadNativePlugins(const QDir &directory)
{
    for (const QString &fileName : directory.entryList(QDir::Files)) {
        QPluginLoader pluginLoader(directory.absoluteFilePath(fileName));
        QObject *plugin = pluginLoader.instance();
        if (!plugin) {
            continue;
        }
        CutterPlugin *cutterPlugin = qobject_cast<CutterPlugin *>(plugin);
        if (!cutterPlugin) {
            continue;
        }
        cutterPlugin->setupPlugin(Core());
        plugins.append(cutterPlugin);
    }
}

#ifdef CUTTER_ENABLE_PYTHON

void PluginManager::loadPythonPlugins(const QDir &directory)
{
    Python()->addPythonPath(directory.absolutePath().toLocal8Bit().data());

    for (const QString &fileName : directory.entryList(QDir::Files)) {
        CutterPlugin *cutterPlugin = nullptr;
        QString moduleName;
        if (fileName.endsWith(".py")) {
            QStringList l = fileName.split(".py");
            moduleName = l[0];
        }
        cutterPlugin = loadPythonPlugin(moduleName.toLocal8Bit().constData());
        if (!cutterPlugin) {
            continue;
        }
        cutterPlugin->setupPlugin(Core());
        plugins.append(cutterPlugin);
    }

    PythonManager::ThreadHolder threadHolder;
}

CutterPlugin *PluginManager::loadPythonPlugin(const char *moduleName)
{
    PythonManager::ThreadHolder threadHolder;
    PyObject *cutterPluginModule = Python()->getCutterPluginModule();
    CutterPythonPlugin *plugin = nullptr;
    if (!cutterPluginModule) {
        return static_cast<CutterPlugin *>(plugin);
    }

    PyObject *pluginModule = PyImport_ImportModule(moduleName);
    if (!pluginModule) {
        qWarning() << "Couldn't load module for plugin:" << QString(moduleName);
        PyErr_PrintEx(10);
    } else {
        plugin = new CutterPythonPlugin(pluginModule);
    }

    return plugin;
}
#endif