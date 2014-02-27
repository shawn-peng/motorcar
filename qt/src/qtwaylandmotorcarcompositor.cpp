/****************************************************************************
**This file is part of the MotorCar QtWayland Compositor, derived from the
**QtWayland QWindow Reference Compositor
**
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
**
****************************************************************************/

#include "qtwaylandmotorcarcompositor.h"
#include "qtwaylandmotorcarsurface.h"



#include <QMouseEvent>
#include <QKeyEvent>
#include <QTouchEvent>
#include <QGuiApplication>
#include <QCursor>
#include <QPixmap>
#include <QScreen>


#include <QtCompositor/qwaylandinput.h>

using namespace qtmotorcar;

QtWaylandMotorcarCompositor::QtWaylandMotorcarCompositor(QOpenGLWindow *window, QGuiApplication *app, motorcar::Scene * scene)
    : QWaylandCompositor(window, 0, DefaultExtensions | SubSurfaceExtension)
    , m_scene(scene)
    , m_glData(new OpenGLData(window))//glm::rotate(glm::translate(glm::mat4(1), glm::vec3(0,0,1.5f)), 180.f, glm::vec3(0,1,0)))))
    , m_renderScheduler(this)
    , m_draggingWindow(0)
    , m_dragKeyIsPressed(false)
    , m_cursorSurface(NULL)
    , m_cursorSurfaceNode(NULL)
    , m_cursorMotorcarSurface(NULL)
    , m_cursorHotspotX(0)
    , m_cursorHotspotY(0)
    , m_modifiers(Qt::NoModifier)
    , m_app(app)
    , m_numSurfacesMapped(0)

{
    setDisplay(NULL);

    m_renderScheduler.setSingleShot(true);
    connect(&m_renderScheduler,SIGNAL(timeout()),this,SLOT(render()));


    window->installEventFilter(this);

    setRetainedSelectionEnabled(true);

    setOutputGeometry(QRect(QPoint(0, 0), window->size()));
    setOutputRefreshRate(qRound(qGuiApp->primaryScreen()->refreshRate() * 1000.0));




//    motorcar::Display testDisplay(window_context, glm::vec2(1), *m_scene, glm::mat4(1));
//    for(int i = 0; i < 2 ; i++){
//        for(int j = 0; j < 2; j++){
//            motorcar::Geometry::printVector(testDisplay.worldPositionAtDisplayPosition(glm::vec2(i * window->size().width(), j * window->size().height())));
//        }
//    }
   //m_renderScheduler.start(0);
    //glClearDepth(0.1f);
}

QtWaylandMotorcarCompositor::~QtWaylandMotorcarCompositor()
{
    delete m_glData;
}

motorcar::Compositor *QtWaylandMotorcarCompositor::create(int argc, char** argv, motorcar::Scene *scene)
{
    // Enable the following to have touch events generated from mouse events.
    // Very handy for testing touch event delivery without a real touch device.
    // QGuiApplication::setAttribute(Qt::AA_SynthesizeTouchForUnhandledMouseEvents, true);

    QGuiApplication *app = new QGuiApplication(argc, argv);
    QScreen *screen;

    if(QGuiApplication::screens().size() ==1){
        screen = QGuiApplication::primaryScreen();
    }else{
        screen =  QGuiApplication::screens().at(1);

    }


    QRect screenGeometry = screen->availableGeometry();

    QSurfaceFormat format;
    format.setDepthBufferSize(16);
    format.setStencilBufferSize(8);

    QRect geom = screenGeometry;
    if (QCoreApplication::arguments().contains(QLatin1String("-nofullscreen")))
        geom = QRect(screenGeometry.width() / 4, screenGeometry.height() / 4,
                     screenGeometry.width() / 2, screenGeometry.height() / 2);

    QOpenGLWindow *window = new QOpenGLWindow(format, geom);
    return  new QtWaylandMotorcarCompositor(window, app, scene);
}

int QtWaylandMotorcarCompositor::start()
{
    this->glData()->m_window->showFullScreen();
    int result = m_app->exec();
    delete m_app;
    return result;
}

motorcar::OpenGLContext *QtWaylandMotorcarCompositor::getContext()
{
    return new QtWaylandMotorcarOpenGLContext(this->glData()->m_window);
}




OpenGLData *QtWaylandMotorcarCompositor::glData() const
{
    return m_glData;
}

void QtWaylandMotorcarCompositor::setGlData(OpenGLData *glData)
{
    m_glData = glData;
}


motorcar::Scene *QtWaylandMotorcarCompositor::scene() const
{
    return m_scene;
}

void QtWaylandMotorcarCompositor::setScene(motorcar::Scene *scene)
{
    m_scene = scene;
}

motorcar::WaylandSurfaceNode *QtWaylandMotorcarCompositor::getSurfaceNode(QWaylandSurface *surface) const
{
    //if passed NULL return first Surface
    if(surface == NULL){
        if(!m_surfaceMap.empty()){
            return m_surfaceMap.begin()->second;
        }else{
            return NULL;
        }
    }
    if(m_surfaceMap.count(surface)){
        return m_surfaceMap.find(surface)->second;
    }
    return NULL;
}



//TODO: consider revising to take  MotorcarSurfaceNode as argument depending on call sites
void QtWaylandMotorcarCompositor::ensureKeyboardFocusSurface(QWaylandSurface *oldSurface)
{
    QWaylandSurface *kbdFocus = defaultInputDevice()->keyboardFocus();
    if (kbdFocus == oldSurface || !kbdFocus){
        motorcar::WaylandSurfaceNode *n = this->getSurfaceNode();
        // defaultInputDevice()->setKeyboardFocus(m_surfaces.isEmpty() ? 0 : m_surfaces.last());
        if(n){
            defaultInputDevice()->setKeyboardFocus(static_cast<QtWaylandMotorcarSurface *>(n->surface())->surface());
        }else{
            defaultInputDevice()->setKeyboardFocus(NULL);
        }

    }
}



void QtWaylandMotorcarCompositor::surfaceDestroyed(QObject *object)
{

    QWaylandSurface *surface = static_cast<QWaylandSurface *>(object);
    //m_surfaces.removeOne(surface);
    if(surface != NULL){ //because calling getSurfaceNode with NULL will cause the first surface node to be returned
        motorcar::WaylandSurfaceNode *surfaceNode = this->getSurfaceNode(surface); //will return surfaceNode whose destructor will remove it from the scenegraph


        if(surfaceNode != NULL){
            scene()->cursorNode()->setValid(false);

            std::vector<motorcar::SceneGraphNode *> subtreeNodes = surfaceNode->nodesInSubtree();
            for(motorcar::SceneGraphNode *node : subtreeNodes){
                motorcar::WaylandSurfaceNode *subtreeSurfaceNode = dynamic_cast<motorcar::WaylandSurfaceNode *>(node);
                if(subtreeSurfaceNode != NULL){
                    QtWaylandMotorcarSurface *subtreeSurface = dynamic_cast<QtWaylandMotorcarSurface *>(subtreeSurfaceNode->surface());
                    if(subtreeSurface != NULL){
                        std::map<QWaylandSurface *, motorcar::WaylandSurfaceNode *>::iterator it = m_surfaceMap.find(subtreeSurface->surface());
                        if (it != m_surfaceMap.end()){
                            std::cout << "nulling surfaceNode pointer: " << it->second  << " in surface map" <<std::endl;
                            it->second = NULL;
                        }
                    }

                }
            }
            m_surfaceMap.erase (surface);

            std::cout << "attempting to delete surfaceNode pointer " << surfaceNode <<std::endl;
            motorcar::Scene *tempScene = new motorcar::Scene();
            //delete surfaceNode;
            surfaceNode->setParentNode(tempScene);
            //surfaceNode->setValid(false);
            defaultInputDevice()->setMouseFocus(NULL, QPointF(0,0));
            defaultInputDevice()->setKeyboardFocus(NULL);


        }

    }
    ensureKeyboardFocusSurface(surface);
    m_renderScheduler.start(0);
}

void QtWaylandMotorcarCompositor::surfaceMapped()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    QPoint pos;
    //if (!m_surfaces.contains(surface)) {
    if (!this->getSurfaceNode(surface)) {
        surface->setPos(QPoint(0, 0));
        if (surface->hasShellSurface()) {

            glm::mat4 transform;
            motorcar::SceneGraphNode *parentNode;
            motorcar::WaylandSurface::SurfaceType surfaceType;

            int type = static_cast<int>(surface->windowType());
            float popupZOffset = 0.05;


            if(type == QWaylandSurface::WindowType::Toplevel){
                int n = 1000;
                int thetaTiles = 3;
                int phiTiles = 2;

                parentNode = this->scene();
                transform = glm::mat4(1)
                                * glm::rotate(glm::mat4(1), -90.f, glm::vec3(0, 1, 0))
                                * glm::translate(glm::mat4(1), glm::vec3(0,0.0,1.25f))
                                * glm::rotate(glm::mat4(1), (-1 +  m_numSurfacesMapped % 3) * 30.f, glm::vec3(0, -1, 0))
                            * glm::rotate(glm::mat4(1), (-1 + m_numSurfacesMapped / 3) * 30.f, glm::vec3(-1, 0, 0))
                            * glm::translate(glm::mat4(1), glm::vec3(0,0.0,-1.5f))
                            * glm::mat4(1);
                surfaceType = motorcar::WaylandSurface::SurfaceType::TOPLEVEL;
                m_numSurfacesMapped ++;
            }else if(type == QWaylandSurface::WindowType::Popup){

                motorcar::WaylandSurfaceNode *surfaceNode = this->getSurfaceNode(this->defaultInputDevice()->mouseFocus());
                parentNode = surfaceNode;
                glm::vec3 position = glm::vec3(surfaceNode->surfaceTransform() *
                                               glm::vec4((surfaceNode->surface()->latestMouseEventPos() +
                                                          glm::vec2(surface->size().width() / 2, surface->size().height() /2)) /
                                                            glm::vec2(surfaceNode->surface()->size()), popupZOffset, 1));
                std::cout << "creating popup window with parent " << surfaceNode << " at position:" << std::endl;
                motorcar::Geometry::printVector(position);
                transform = glm::translate(glm::mat4(), position);
                surfaceType = motorcar::WaylandSurface::SurfaceType::POPUP;
            }else if(type == QWaylandSurface::WindowType::Transient){
                transform = glm::translate(glm::mat4(), glm::vec3(0,0,popupZOffset));
                parentNode = this->getSurfaceNode(this->defaultInputDevice()->mouseFocus());
                surfaceType = motorcar::WaylandSurface::SurfaceType::TRANSIENT;
            }else{
                transform = glm::translate(glm::mat4(), glm::vec3(0,0,popupZOffset));
                parentNode = this->getSurfaceNode(this->defaultInputDevice()->mouseFocus());
                surfaceType = motorcar::WaylandSurface::SurfaceType::NA;
            }


            motorcar::WaylandSurfaceNode *surfaceNode = new motorcar::WaylandSurfaceNode(new QtWaylandMotorcarSurface(surface, this, surfaceType), parentNode, transform);
            defaultInputDevice()->setKeyboardFocus(surface);

            m_surfaceMap.insert(std::pair<QWaylandSurface *, motorcar::WaylandSurfaceNode *>(surface, surfaceNode));

            std::cout << "surface parameters: "
                      << surface->hasInputPanelSurface() << ", "
                      << surface->isWidgetType() << ", "
                      << surface->isWindowType() << ", "
                      << surface->type() << ", "
                      << surface->windowType() << ", "
                      << surface << ", "
                      << "<" << surface->pos().x() << ", " << surface->pos().y() << ">" << std::endl;


    }



    }

    m_renderScheduler.start(0);
}

void QtWaylandMotorcarCompositor::surfaceUnmapped()
{

    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    //    if (m_surfaces.removeOne(surface))
    //        m_surfaces.insert(0, surface);

    ensureKeyboardFocusSurface(surface);

    //m_renderScheduler.start(0);
}

void QtWaylandMotorcarCompositor::surfaceDamaged(const QRect &rect)
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    surfaceDamaged(surface, rect);
}

void QtWaylandMotorcarCompositor::surfacePosChanged()
{
    m_renderScheduler.start(0);
}

void QtWaylandMotorcarCompositor::surfaceDamaged(QWaylandSurface *surface, const QRect &rect)
{
    Q_UNUSED(surface)
    Q_UNUSED(rect)
    m_renderScheduler.start(0);
}

void QtWaylandMotorcarCompositor::surfaceCreated(QWaylandSurface *surface)
{
    connect(surface, SIGNAL(destroyed(QObject *)), this, SLOT(surfaceDestroyed(QObject *)));
    connect(surface, SIGNAL(mapped()), this, SLOT(surfaceMapped()));
    connect(surface, SIGNAL(unmapped()), this, SLOT(surfaceUnmapped()));
    connect(surface, SIGNAL(damaged(const QRect &)), this, SLOT(surfaceDamaged(const QRect &)));
    connect(surface, SIGNAL(extendedSurfaceReady()), this, SLOT(sendExpose()));
    connect(surface, SIGNAL(posChanged()), this, SLOT(surfacePosChanged()));
    m_renderScheduler.start(0);
}

void QtWaylandMotorcarCompositor::sendExpose()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    surface->sendOnScreenVisibilityChange(true);
}



QPointF QtWaylandMotorcarCompositor::toSurface(QWaylandSurface *surface, const QPointF &point) const
{
    motorcar::WaylandSurfaceNode *surfaceNode = this->getSurfaceNode(surface);

    if(surfaceNode != NULL){
        motorcar::Geometry::Ray ray = display()->worldRayAtDisplayPosition(glm::vec2(point.x(), point.y()));
        ray = ray.transform(glm::inverse(surfaceNode->worldTransform()));
        float t;
        glm::vec2 intersection;
        bool isIntersected = surfaceNode->computeLocalSurfaceIntersection(ray, intersection, t);
        if(isIntersected){
            return QPointF(intersection.x, intersection.y);
        }else{
            qDebug() << "ERROR: surface plane does not interesect camera ray through cursor";
            return QPointF();
        }

    }else{
        qDebug() << "ERROR: could not find SceneGraphNode for the given Surface";
        return QPointF();
    }
}

void QtWaylandMotorcarCompositor::updateCursor()
{
    if (!m_cursorSurface)
        return;
    QCursor cursor(QPixmap::fromImage(m_cursorSurface->image()), m_cursorHotspotX, m_cursorHotspotY);
    static bool cursorIsSet = false;
    if (cursorIsSet) {
        QGuiApplication::changeOverrideCursor(cursor);
    } else {
        QGuiApplication::setOverrideCursor(cursor);
        cursorIsSet = true;
    }
}

void QtWaylandMotorcarCompositor::setCursorSurface(QWaylandSurface *surface, int hotspotX, int hotspotY)
{

    if(m_cursorSurfaceNode == NULL){
        m_cursorMotorcarSurface =new QtWaylandMotorcarSurface(surface, this, motorcar::WaylandSurface::SurfaceType::CURSOR);
        m_cursorSurfaceNode =  new motorcar::WaylandSurfaceNode(m_cursorMotorcarSurface, m_scene, glm::rotate(glm::mat4(1), -90.f, glm::vec3(0, 1, 0)));
        m_scene->setCursorNode(m_cursorSurfaceNode);
    }

    m_cursorMotorcarSurface->setSurface(surface);
    m_scene->setCursorHotspot(glm::ivec2(hotspotX, hotspotY));


    if ((m_cursorSurface != surface) && surface){
        connect(surface, SIGNAL(damaged(QRect)), this, SLOT(updateCursor()));
    }

    m_cursorSurface = surface;
    m_cursorHotspotX = hotspotX;
    m_cursorHotspotY = hotspotY;
}


QWaylandSurface *QtWaylandMotorcarCompositor::surfaceAt(const QPointF &point, QPointF *local)
{
    motorcar::Geometry::Ray ray = display()->worldRayAtDisplayPosition(glm::vec2(point.x(), point.y()));
    motorcar::Geometry::RaySurfaceIntersection *intersection = m_scene->intersectWithSurfaces(ray);

    if(intersection){
        //qDebug() << "intersection found between cursor ray and scene graph";
        if (local){
            *local = QPointF(intersection->surfaceLocalCoordinates.x, intersection->surfaceLocalCoordinates.y);
        }
        motorcar::WaylandSurface *surface = intersection->surfaceNode->surface();
        delete intersection;

        return static_cast<QtWaylandMotorcarSurface *>(surface)->surface();

    }else{
        //qDebug() << "no intersection found between cursor ray and scene graph";
        return NULL;
    }


}


void QtWaylandMotorcarCompositor::render()
{


    //   m_glData->m_backgroundTexture = m_glData->m_textureCache->bindTexture(QOpenGLContext::currentContext(),m_glData->m_backgroundImage);

    //    m_glData->m_textureBlitter->bind();
    //    // Draw the background image texture
    //    m_glData->m_textureBlitter->drawTexture(m_glData->m_backgroundTexture,
    //                                  QRect(QPoint(0, 0), m_glData->m_backgroundImage.size()),
    //                                  m_glData->m_window->size(),
    //                                  0, false, true);
    //    m_glData->m_textureBlitter->release();



    scene()->draw(0);

    frameFinished();
    // N.B. Never call glFinish() here as the busylooping with vsync 'feature' of the nvidia binary driver is not desirable.
    m_glData->m_window->swapBuffers();
    m_renderScheduler.start(0);
}

bool QtWaylandMotorcarCompositor::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != m_glData->m_window)
        return false;

    QWaylandInputDevice *input = defaultInputDevice();

    switch (event->type()) {
    case QEvent::Expose:
        m_renderScheduler.start(0);
        if (m_glData->m_window->isExposed()) {
            // Alt-tabbing away normally results in the alt remaining in
            // pressed state in the clients xkb state. Prevent this by sending
            // a release. This is not an issue in a "real" compositor but
            // is very annoying when running in a regular window on xcb.
            Qt::KeyboardModifiers mods = QGuiApplication::queryKeyboardModifiers();
            if (m_modifiers != mods && input->keyboardFocus()) {
                Qt::KeyboardModifiers stuckMods = m_modifiers ^ mods;
                if (stuckMods & Qt::AltModifier)
                    input->sendKeyReleaseEvent(64); // native scancode for left alt
                m_modifiers = mods;
            }
        }
        break;
    case QEvent::MouseButtonPress: {
        QPointF local;
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        QWaylandSurface *targetSurface = surfaceAt(me->localPos(), &local);
        if (m_dragKeyIsPressed && targetSurface) {
            m_draggingWindow = targetSurface;
            m_drag_diff = local;
        } else {
            if (targetSurface && input->keyboardFocus() != targetSurface) {
                input->setKeyboardFocus(targetSurface);
                //                m_surfaces.removeOne(targetSurface);
                //                m_surfaces.append(targetSurface);
                m_renderScheduler.start(0);
            }
            input->sendMousePressEvent(me->button(), local, me->localPos());
        }
        return true;
    }
    case QEvent::MouseButtonRelease: {
        QWaylandSurface *targetSurface = input->mouseFocus();
        if (m_draggingWindow) {
            m_draggingWindow = 0;
            m_drag_diff = QPointF();
        } else {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            QPointF localPos;
            if (targetSurface)
                localPos = toSurface(targetSurface, me->localPos());
            input->sendMouseReleaseEvent(me->button(), localPos, me->localPos());
        }
        return true;
    }
    case QEvent::MouseMove: {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (m_draggingWindow) {
            m_draggingWindow->setPos(me->localPos() - m_drag_diff);
            m_renderScheduler.start(0);
        } else {
            QPointF local;
            QWaylandSurface *targetSurface = surfaceAt(me->localPos(), &local);
            input->sendMouseMoveEvent(targetSurface, local, me->localPos());
        }
        break;
    }
    case QEvent::Wheel: {
        QWheelEvent *we = static_cast<QWheelEvent *>(event);
        input->sendMouseWheelEvent(we->orientation(), we->delta());
        break;
    }
    case QEvent::KeyPress: {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Meta || ke->key() == Qt::Key_Super_L) {
            m_dragKeyIsPressed = true;
        }/*else if(ke->key() == Qt::Key_Up){
            m_glData->m_cameraNode->setTransform(glm::translate(glm::mat4(1), glm::vec3(0,0,0.001f)) * m_glData->m_cameraNode->transform());
        }else if(ke->key() == Qt::Key_Down){
            m_glData->m_cameraNode->setTransform(glm::translate(glm::mat4(1), glm::vec3(0,0,-0.001f)) * m_glData->m_cameraNode->transform());
        }*/
        m_modifiers = ke->modifiers();
        QWaylandSurface *targetSurface = input->keyboardFocus();
        if (targetSurface)
            input->sendKeyPressEvent(ke->nativeScanCode());
        break;
    }
    case QEvent::KeyRelease: {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Meta || ke->key() == Qt::Key_Super_L) {
            m_dragKeyIsPressed = false;
        }
        m_modifiers = ke->modifiers();
        QWaylandSurface *targetSurface = input->keyboardFocus();
        if (targetSurface)
            input->sendKeyReleaseEvent(ke->nativeScanCode());
        break;
    }
        //    case QEvent::TouchBegin:
        //    case QEvent::TouchUpdate:
        //    case QEvent::TouchEnd:
        //    {
        //        QWaylandSurface *targetSurface = 0;
        //        QTouchEvent *te = static_cast<QTouchEvent *>(event);
        //        QList<QTouchEvent::TouchPoint> points = te->touchPoints();
        //        QPoint pointPos;
        //        if (!points.isEmpty()) {
        //            pointPos = points.at(0).pos().toPoint();
        //            targetSurface = surfaceAt(pointPos);
        //        }
        //        if (targetSurface && targetSurface != input->mouseFocus())
        //            input->setMouseFocus(targetSurface, pointPos, pointPos);
        //        if (input->mouseFocus())
        //            input->sendFullTouchEvent(te);
        //        break;
        //    }
    default:
        break;
    }
    return false;
}





