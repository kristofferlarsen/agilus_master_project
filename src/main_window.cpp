//
// Original author Asgeir Bjoerkedal & Kristoffer Larsen. Latest change date 01.05.2016
// main_window.cpp is responsible for all user interraction and information flow.
//
// Created as part of the software solution for a Master's Thesis in Production Technology at NTNU Trondheim.
//

#include <QtGui>
#include <QMessageBox>
#include <iostream>
#include "../include/agilus_master_project/main_window.hpp"

namespace agilus_master_project {

using namespace Qt;

MainWindow::MainWindow(int argc, char** argv, QWidget *parent)
    : QMainWindow(parent)
    , qnode(argc,argv)
    , currentCloud(new pcl::PointCloud<pcl::PointXYZ>)
    , partApath(ros::package::getPath("agilus_master_project")+"/resources/parts/org2.png")
    , partBpath(ros::package::getPath("agilus_master_project")+"/resources/parts/bottom2.png"){
    qRegisterMetaType<pcl::PointCloud<pcl::PointXYZ>::Ptr >("pcl::PointCloud<pcl::PointXYZ>::Ptr");
    qRegisterMetaType<cv::Mat>("cv::Mat");
    ui.setupUi(this);
    // Connecting Signals and slots
    QObject::connect(&qnode, SIGNAL(rosShutdown()), this, SLOT(close()));
    QObject::connect(this,SIGNAL(subscribeToPointCloud2(QString)),&qnode,SLOT(subscribeToPointCloud2(QString)));
    QObject::connect(this,SIGNAL(subscribeTo2DobjectDetected(QString)),&qnode,SLOT(subscribeTo2DobjectDetected(QString)));
    QObject::connect(&qnode,SIGNAL(updatePointCloud(pcl::PointCloud<pcl::PointXYZ>::Ptr)),this,SLOT(updatePointCloud(pcl::PointCloud<pcl::PointXYZ>::Ptr)));
    QObject::connect(&qnode,SIGNAL(update2Dimage(QImage)),this,SLOT(update2Dimage(QImage)));
    QObject::connect(this,SIGNAL(setProcessImageRunning(bool)),&qnode,SLOT(setProcessImageRunning(bool)));
    QObject::connect(this,SIGNAL(setProcessImageColor(bool)),&qnode,SLOT(setProcessImageColor(bool)));
    QObject::connect(this,SIGNAL(setProcessImageUndistort(bool)),&qnode,SLOT(setProcessImageUndistort(bool)));
    QObject::connect(this,SIGNAL(setProcessImageBruteforce(bool)),&qnode,SLOT(setProcessImageBruteforce(bool)));
    QObject::connect(this,SIGNAL(setProcessImageKeypointDescriptor(std::string,std::string)),&qnode,SLOT(setProcessImageKeypointDescriptor(std::string,std::string)));
    QObject::connect(this,SIGNAL(setProcessImageMatchingPicture(std::string)),&qnode,SLOT(setProcessImageMatchingPicture(std::string)));
    QObject::connect(this,SIGNAL(plan_ag1(double,double,double,double,double,double)),&qnode,SLOT(plan_ag1(double,double,double,double,double,double)));
    QObject::connect(this,SIGNAL(plan_ag2(double,double,double,double,double,double)),&qnode,SLOT(plan_ag2(double,double,double,double,double,double)));
    QObject::connect(this,SIGNAL(move_ag1(double,double,double,double,double,double)),&qnode,SLOT(move_ag1(double,double,double,double,double,double)));
    QObject::connect(this,SIGNAL(move_ag2(double,double,double,double,double,double)),&qnode,SLOT(move_ag2(double,double,double,double,double,double)));
    QObject::connect(this,SIGNAL(setProcessImageDepthLambda(double)),&qnode,SLOT(setProcessImageDepthLambda(double)));
    QObject::connect(this,SIGNAL(closeAG1Gripper()),&qnode,SLOT(closeAG1Gripper()));
    QObject::connect(this,SIGNAL(openAG1Gripper()),&qnode,SLOT(openAG1Gripper()));
    QObject::connect(&qnode,SIGNAL(disableAuto()),this,SLOT(disableAuto()));
    //Initialize local data
    init();
    init_descriptor_keypoint_combobox();
    initRobotUI();
}

MainWindow::~MainWindow() {}

void MainWindow::closeEvent(QCloseEvent *event){
    QMainWindow::closeEvent(event);
}

void MainWindow::displayNewPointCloud(boost::shared_ptr<pcl::visualization::PCLVisualizer> vis){
    runStream = false;
    viewer1->getCameras(cam);
    viewer1 = vis;
    w1->SetRenderWindow(viewer1->getRenderWindow());
    viewer1->setCameraPosition(cam[0].pos[0],cam[0].pos[1],cam[0].pos[2],cam[0].view[0],cam[0].view[1],cam[0].view[2]);
    drawShapes();
}

void MainWindow::drawShapes(){
    viewer1->removeAllShapes();
    if(ui.boxesCheckbox->isChecked()){
        viewer1->addCube(-0.510, -0.130, -0.5, 0.3, 0.76, 1.19, 0, 1.0, 0, "partA", 0);
        viewer1->addCube(-0.130, 0.27, -0.5, 0.3, 0.76, 1.19, 1.0, 0, 0, "partB", 0);
    }
    if(ui.framesCheckbox->isChecked()){
        viewer1->addCoordinateSystem(0.2,tag);
        viewer1->addCoordinateSystem(0.2,camera);
        viewer1->addCoordinateSystem(0.5,worldFrame);
    }
    w1->update();
}

void MainWindow::init(){
    ui.outputTabManager->setCurrentIndex(0);
    ui.settingsTabManager->setCurrentIndex(0);
    filters = new PclFilters();
    runStream = false;
    movedToPartA = false;
    movedToPartB = false;
    box = new ModelLoader("nuc-42-100");
    box->populateLoader();
    cone = new ModelLoader("cone-42-200");
    cone->populateLoader();
    freak = new ModelLoader("freakthing-42-200");
    freak->populateLoader();
    models.push_back(box);
    models.push_back(cone);
    models.push_back(freak);
    w1 = new QVTKWidget();
    viewer1.reset(new pcl::visualization::PCLVisualizer ("3D Viewer", false));
    w1->SetRenderWindow(viewer1->getRenderWindow());
    viewer1->setCameraPosition( 0.146598, 0.0941454, -4.95334, -0.0857047, -0.0396425, 0.600109, -0.00146821, -0.999707, -0.0241453, 0);
    ui.pointCloudLayout->addWidget(w1);
    cameraToTag << -0.000762713, -0.999832,   -0.0182927, -0.116073,
            -0.569249,     0.0154738,  -0.822019,  -0.0416634,
            0.822165,     0.00978617, -0.569166,   1.0405,
            0,            0,           0,          1;
    tag = cameraToTag;
    world << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1;
    worldToTag << 1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0.87,
                0, 0, 0, 1;
    tagToWorld = worldToTag.inverse();
    tagToCamera = cameraToTag.inverse();
    tempWorld = cameraToTag*tagToWorld;
    worldFrame = tempWorld;
    camera = world;
    drawShapes();
    qnode.init();
    objects.append("nuc box");
    objects.append("cone");
    objects.append("freak thing");
    ui.objectsListA->addItems(objects);
    ui.objectsListB->addItems(objects);
    ui.objectsListA->setCurrentIndex(2);
    ui.objectsListB->setCurrentIndex(1);
    QStringList list, list2;
    QString tmp;
    ros::master::V_TopicInfo master_topics;
    ros::master::getTopics(master_topics);
    for (ros::master::V_TopicInfo::iterator it = master_topics.begin() ; it != master_topics.end(); it++) {
        const ros::master::TopicInfo& info = *it;
        //std::cout << "Topic : " << it - master_topics.begin() << ": " << info.name << " -> " << info.datatype <<       std::endl;
        tmp = QString::fromUtf8(info.datatype.c_str());
        // Add more types if needed
        if(QString::compare(tmp, "sensor_msgs/PointCloud2", Qt::CaseInsensitive) == 0){
            list.append(QString::fromUtf8(info.name.c_str()));
        }
    }
    ui.topicComboBox->addItems(list);
    for (ros::master::V_TopicInfo::iterator it = master_topics.begin() ; it != master_topics.end(); it++) {
        const ros::master::TopicInfo& info = *it;
        //std::cout << "Topic : " << it - master_topics.begin() << ": " << info.name << " -> " << info.datatype <<       std::endl;
        tmp = QString::fromUtf8(info.datatype.c_str());

        // Add more types if needed
        if(QString::compare(tmp, "sensor_msgs/Image", Qt::CaseInsensitive) == 0){
            QString tmp = QString::fromUtf8(info.name.c_str());
            if(tmp.endsWith("image_color")){
                list2.append(QString::fromUtf8(info.name.c_str()));
            } else if(tmp.endsWith("image")){
                list2.append(QString::fromUtf8(info.name.c_str()));
            }
        }
    }
    ui.topicComboBox2->addItems(list2);
    ui.logViewer->insertPlainText("User interface initiated");
    QStringList raytraceResolution;
    QStringList raytraceTesselationLevel;
    raytraceResolution.append("50");
    raytraceResolution.append("100");
    raytraceResolution.append("200");
    raytraceResolution.append("300");
    raytraceResolution.append("400");
    raytraceResolution.append("500");
    raytraceResolution.append("600");
    raytraceResolution.append("700");
    raytraceResolution.append("800");
    raytraceResolution.append("900");
    raytraceTesselationLevel.append("42");
    raytraceTesselationLevel.append("162");
    raytraceTesselationLevel.append("642");
    ui.resolution_combobox->addItems(raytraceResolution);
    ui.tesselation_level_combobox->addItems(raytraceTesselationLevel);
}

void MainWindow::init_descriptor_keypoint_combobox(){
    QStringList keyPointLabels;
    QStringList descriptorLabels;
    descriptorLabels.append("SIFT");
    descriptorLabels.append("SURF");
    descriptorLabels.append("BRIEF");
    descriptorLabels.append("BRISK");
    descriptorLabels.append("FREAK");
    descriptorLabels.append("ORB");
    descriptorLabels.append("AKAZE");
    keyPointLabels.append("SIFT");
    keyPointLabels.append("SURF");
    keyPointLabels.append("FAST");
    keyPointLabels.append("STAR");
    keyPointLabels.append("BRISK");
    keyPointLabels.append("ORB");
    keyPointLabels.append("AKAZE");
    ui.descriptorComboBox->addItems(descriptorLabels);
    ui.keypointComboBox->addItems(keyPointLabels);
    ui.keypointComboBox->setCurrentIndex(0);
    ui.descriptorComboBox->setCurrentIndex(0);
}

void MainWindow::initRobotUI(){
    QStringList manipulators;
    manipulators.append("KUKA Agilus 1");
    manipulators.append("KUKA Agilus 2");
    ui.robotComboBox->addItems(manipulators);
    homeX = 0.445;
    homeY1 = -0.6025;
    homeY2 = 0.6025;
    homeZ = 1.66;
    homeRoll = 0.0;
    homePitch = 180.0;
    homeYaw = 0.0;
}

void MainWindow::detect3D(int partAIndex, int partBIndex){
    //detect objects here
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr detected(new pcl::PointCloud<pcl::PointXYZRGB>);
    icpResult objectDetectionResult = filters->object_detection(currentCloud,models.at(partAIndex)->getModels(),models.at(partBIndex)->getModels());
    displayNewPointCloud(filters->visualize_rgb(objectDetectionResult.cloud));

    frameA = objectDetectionResult.partAFinal;
    frameB = objectDetectionResult.partBFinal;

    partAInTag = world * worldToTag * tagToCamera * objectDetectionResult.partAFinal;
    partBInTag = world * worldToTag * tagToCamera * objectDetectionResult.partBFinal;

    viewer1->addCoordinateSystem(0.2,frameA);
    viewer1->addCoordinateSystem(0.2,frameB);

    QString partA = "X: ";
    partA.append(QString::number(partAInTag(0,3)));
    partA.append(", Y: ");
    partA.append(QString::number(partAInTag(1,3)));
    partA.append(", Z: ");
    partA.append(QString::number(partAInTag(2,3)));

    QString partB = "X: ";
    partB.append(QString::number(partBInTag(0,3)));
    partB.append(", Y: ");
    partB.append(QString::number(partBInTag(1,3)));
    partB.append(", Z: ");
    partB.append(QString::number(partBInTag(2,3)));

    printToLog("Position of part a in world:");
    printToLog(partA);
    printToLog("Position of part b in world");
    printToLog(partB);

    ui.plan2DcorrButton->setEnabled(true);
    ui.move2DcorrButton->setEnabled(true);
}

void MainWindow::detectAngle2D(double &part){
    int counter = qnode.getCounter();
    std::vector<double> temp;
    temp.push_back(qnode.getTheta());
    while(true){
        while(qnode.getCounter() == counter);
        counter = qnode.getCounter();
        temp.push_back(qnode.getTheta());
        if(temp.size() > 20){
            double d = 0.0;
            for(int i=0; i<temp.size(); i++) {
                d = d + temp.at(i);
            }
            double average = d/(double)temp.size();
            part = average;
            return;
        }
    }
}

void MainWindow::updatePointCloud(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud){
    if(runStream){
        viewer1->updatePointCloud(cloud,"sample cloud");
        if(!viewer1->updatePointCloud(cloud,"sample cloud")){
            viewer1->addPointCloud(cloud,"sample cloud");
        }
        w1->update();
        *currentCloud = *cloud;
    }
}

void MainWindow::update2Dimage(QImage image){
    saveImage = image;
    ui.label2D->setPixmap(QPixmap::fromImage(image));
    qnode.setImageReading(false);
}

void MainWindow::printToLog(QString text){
    QString header = "\n";
    header.append(text);
    ui.logViewer->moveCursor(QTextCursor::End);
    ui.logViewer->insertPlainText(header);
}

void MainWindow::on_saveImageButton_clicked(bool check){
    QImage tmpImage = saveImage;
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Image"), "/home/minions",tr("Image (*.png)"));
    if(fileName.length() != 0) {
        if(!fileName.endsWith(".png",Qt::CaseInsensitive)){
            fileName.append(".png");
        }
        QString tmpstring = "Saving .png to ";
        tmpstring.append(fileName);
        printToLog(tmpstring);
        tmpImage.save(fileName);
    }
}

void MainWindow::on_boxesCheckbox_clicked(bool check){
    drawShapes();
}

void MainWindow::on_framesCheckbox_clicked(bool check){
    drawShapes();
}

void MainWindow::on_runningCheckBox_clicked(bool check){
    Q_EMIT setProcessImageRunning(check);
    if(check) {
        printToLog("Image detection running");
    } else {
        printToLog("Image detection not running");
    }
}

void MainWindow::on_colorCheckBox_clicked(bool check){
    Q_EMIT setProcessImageColor(check);
    if(check) {
        printToLog("Color image");
    } else {
        printToLog("Grayscale image");
    }
}

void MainWindow::on_undistortCheckBox_clicked(bool check){
    Q_EMIT setProcessImageUndistort(check);
    if(check) {
        printToLog("Distorted image");
    } else {
        printToLog("Undistorted image");
    }
}

void MainWindow::on_bruteforceCheckBox_clicked(bool check){
    Q_EMIT setProcessImageBruteforce(check);
    if(check) {
        printToLog("Bruteforce matching");
    } else {
        printToLog("FLANN Matching");
    }
}

void MainWindow::on_updateSettingsPushButton_clicked(bool check){
    Q_EMIT setProcessImageKeypointDescriptor(ui.keypointComboBox->currentText().toStdString(), ui.descriptorComboBox->currentText().toStdString());
}

void MainWindow::on_imageToDetectButton_clicked(bool check){
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),"/home/minions",tr("png (*.png);;jpg (*.jpg *.jpeg)"));
    if(fileName.length() != 0) {
        QString tmpstring = "Loading matching image from ";
        tmpstring.append(fileName);
        printToLog(tmpstring);
        Q_EMIT setProcessImageMatchingPicture(fileName.toStdString());
    }
}

void MainWindow::on_planTrajPushButton_clicked(bool check){
    if(ui.robotComboBox->currentIndex()==0) {
        if(ui.worldCoordinatesCheckBox->isChecked()) {
            Q_EMIT plan_ag1(ui.xPosDoubleSpinBox->value(),
                            ui.yPosDoubleSpinBox->value(),
                            ui.zPosDoubleSpinBox->value(),
                            ui.rollDoubleSpinBox->value()*(M_PI/180.0),
                            ui.pitchDoubleSpinBox->value()*(M_PI/180.0),
                            ui.yawDoubleSpinBox->value()*(M_PI/180.0));
        } else {
            Q_EMIT plan_ag1(ui.xPosDoubleSpinBox->value()+homeX,
                            ui.yPosDoubleSpinBox->value()+homeY1,
                            ui.zPosDoubleSpinBox->value()+homeZ,
                            ((homeRoll+ui.rollDoubleSpinBox->value())*M_PI)/180.0,
                            ((homePitch+ui.pitchDoubleSpinBox->value())*M_PI)/180.0,
                            ((homeYaw+ui.yawDoubleSpinBox->value())*M_PI)/180.0);
        }
    } else if(ui.robotComboBox->currentIndex()==1) {
        if(ui.worldCoordinatesCheckBox->isChecked()) {
            Q_EMIT plan_ag2(ui.xPosDoubleSpinBox->value(),
                            ui.yPosDoubleSpinBox->value(),
                            ui.zPosDoubleSpinBox->value(),
                            ui.rollDoubleSpinBox->value()*(M_PI/180.0),
                            ui.pitchDoubleSpinBox->value()*(M_PI/180.0),
                            ui.yawDoubleSpinBox->value()*(M_PI/180.0));
        } else {
            Q_EMIT plan_ag2(ui.xPosDoubleSpinBox->value()+homeX,
                            ui.yPosDoubleSpinBox->value()+homeY2,
                            ui.zPosDoubleSpinBox->value()+homeZ,
                            ((homeRoll+ui.rollDoubleSpinBox->value())*M_PI)/180.0,
                            ((homePitch+ui.pitchDoubleSpinBox->value())*M_PI)/180.0,
                            ((homeYaw+ui.yawDoubleSpinBox->value())*M_PI)/180.0);
        }
    }
}

void MainWindow::on_moveManipPushButton_clicked(bool check){
    if(ui.robotComboBox->currentIndex()==0) {
        if(ui.worldCoordinatesCheckBox->isChecked()) {
            Q_EMIT move_ag1(ui.xPosDoubleSpinBox->value(),
                            ui.yPosDoubleSpinBox->value(),
                            ui.zPosDoubleSpinBox->value(),
                            ui.rollDoubleSpinBox->value()*(M_PI/180.0),
                            ui.pitchDoubleSpinBox->value()*(M_PI/180.0),
                            ui.yawDoubleSpinBox->value()*(M_PI/180.0));
        } else {
            Q_EMIT move_ag1(ui.xPosDoubleSpinBox->value()+homeX,
                            ui.yPosDoubleSpinBox->value()+homeY1,
                            ui.zPosDoubleSpinBox->value()+homeZ,
                            ((homeRoll+ui.rollDoubleSpinBox->value())*M_PI)/180.0,
                            ((homePitch+ui.pitchDoubleSpinBox->value())*M_PI)/180.0,
                            ((homeYaw+ui.yawDoubleSpinBox->value())*M_PI)/180.0);
        }
    } else if(ui.robotComboBox->currentIndex()==1) {
        if(ui.worldCoordinatesCheckBox->isChecked()) {
            Q_EMIT move_ag2(ui.xPosDoubleSpinBox->value(),
                            ui.yPosDoubleSpinBox->value(),
                            ui.zPosDoubleSpinBox->value(),
                            ui.rollDoubleSpinBox->value()*(M_PI/180.0),
                            ui.pitchDoubleSpinBox->value()*(M_PI/180.0),
                            ui.yawDoubleSpinBox->value()*(M_PI/180.0));
        } else {
            Q_EMIT move_ag2(ui.xPosDoubleSpinBox->value()+homeX,
                            ui.yPosDoubleSpinBox->value()+homeY2,
                            ui.zPosDoubleSpinBox->value()+homeZ,
                            ((homeRoll+ui.rollDoubleSpinBox->value())*M_PI)/180.0,
                            ((homePitch+ui.pitchDoubleSpinBox->value())*M_PI)/180.0,
                            ((homeYaw+ui.yawDoubleSpinBox->value())*M_PI)/180.0);
        }
    }
}

void MainWindow::on_homeManipPushButton_clicked(bool check){
    ui.xPosDoubleSpinBox->setValue(0.0);
    ui.yPosDoubleSpinBox->setValue(0.0);
    ui.zPosDoubleSpinBox->setValue(0.0);
    ui.rollDoubleSpinBox->setValue(0.0);
    ui.pitchDoubleSpinBox->setValue(0.0);
    ui.yawDoubleSpinBox->setValue(0.0);
}

void MainWindow::on_plan2DcorrButton_clicked(bool check){
    double correctionY = partAInTag(0,3)-qnode.getYoffset();
    double correctionX = partAInTag(1,3)-qnode.getXoffset();
    Q_EMIT plan_ag2(correctionY,correctionX,1.5,0,3.1415,(-23.5)*(M_PI/180.0));
}

void MainWindow::on_move2DcorrButton_clicked(bool check){
    double correctionY = partAInTag(0,3)-qnode.getYoffset();
    double correctionX = partAInTag(1,3)-qnode.getXoffset();
    Q_EMIT move_ag2(correctionY,correctionX,1.5,0,3.1415,(-23.5)*(M_PI/180.0));
    partAInTag(0,3) = correctionY;
    partAInTag(1,3) = correctionX;
}

void MainWindow::on_setDepthPushButton_clicked(bool check){
    Q_EMIT setProcessImageDepthLambda(ui.lambdaDoubleSpinBox->value());
}

void MainWindow::on_openGripperButton_clicked(bool check){
    if(ui.robotComboBox->currentIndex()==0) {
        Q_EMIT openAG1Gripper();
    }
}

void MainWindow::on_closeGripperButton_clicked(bool check){
    if(ui.robotComboBox->currentIndex()==0) {
        Q_EMIT closeAG1Gripper();
    }
}

void MainWindow::on_autoDetection3dButton_clicked(bool check){
    detect3D(2,1);
}

void MainWindow::on_auto2dFirstPartButton_clicked(bool check){
    if(!movedToPartA) {
        Q_EMIT move_ag1(homeX,homeY1,homeZ,homeRoll,homePitch*(M_PI/180.0),homeYaw);
        Q_EMIT move_ag2(0.35,0.20,1.66,0,3.1415,(-23.5)*(M_PI/180.0));
        Q_EMIT move_ag2(partAInTag(0,3),partAInTag(1,3),1.66,0,3.1415,(-23.5)*(M_PI/180.0));
        Q_EMIT move_ag2(partAInTag(0,3),partAInTag(1,3),1.35,0,3.1415,(-23.5)*(M_PI/180.0));
        Q_EMIT setProcessImageKeypointDescriptor("SIFT", "SIFT");
        Q_EMIT setProcessImageMatchingPicture(partApath);
        Q_EMIT setProcessImageDepthLambda(0.138);
        movedToPartA = true;
    } else {
        double correctionY = partAInTag(0,3)-qnode.getYoffset();
        double correctionX = partAInTag(1,3)-qnode.getXoffset();
        Q_EMIT move_ag2(correctionY,correctionX,1.35,0,3.1415,(-23.5)*(M_PI/180.0));
        partAInTag(0,3) = correctionY;
        partAInTag(1,3) = correctionX;
    }
}

void MainWindow::on_auto2dFirstPartAngleButton_clicked(bool check){
    //correct partAinTag to be correct in robot world frames
    geometry_msgs::PoseStamped ag2Pos = qnode.getAg2CurrentPose();
    partAInTag(0,3) = ag2Pos.pose.position.x + 0.007;
    partAInTag(1,3) = ag2Pos.pose.position.y + 0.001;
    detectAngle2D(anglePartA);
    QString averageString = "Average angle part A: ";
    averageString.append(QString::number(anglePartA));
    printToLog(averageString);
}

void MainWindow::on_auto2dSecondPartButton_clicked(bool check){
    if(!movedToPartB) {
        Q_EMIT move_ag1(homeX,homeY1,homeZ,homeRoll,homePitch*(M_PI/180.0),homeYaw);
        Q_EMIT move_ag2(partAInTag(0,3),partAInTag(1,3),1.45,0,3.1415,(-23.5)*(M_PI/180.0));
        Q_EMIT move_ag2(partBInTag(0,3),partBInTag(1,3),1.45,0,3.1415,(-23.5)*(M_PI/180.0));
        Q_EMIT move_ag2(partBInTag(0,3),partBInTag(1,3),1.39,0,3.1415,(-23.5)*(M_PI/180.0));
        Q_EMIT setProcessImageKeypointDescriptor("SIFT", "SIFT");
        Q_EMIT setProcessImageMatchingPicture(partBpath);
        Q_EMIT setProcessImageDepthLambda(0.153);
        movedToPartB = true;
    } else {
        double correctionY = partBInTag(0,3)-qnode.getYoffset();
        double correctionX = partBInTag(1,3)-qnode.getXoffset();
        Q_EMIT move_ag2(correctionY,correctionX,1.39,0,3.1415,(-23.5)*(M_PI/180.0));
        partBInTag(0,3) = correctionY;
        partBInTag(1,3) = correctionX;
    }
}

void MainWindow::on_auto2dSecondPartAngleButton_clicked(bool check){
    geometry_msgs::PoseStamped ag2Pos = qnode.getAg2CurrentPose();
    partBInTag(0,3) = ag2Pos.pose.position.x + 0.007;
    partBInTag(1,3) = ag2Pos.pose.position.y + 0.001;
    detectAngle2D(anglePartB);
    QString averageString = "Average angle part B: ";
    averageString.append(QString::number(anglePartB));
    printToLog(averageString);
}

void MainWindow::on_moveGripperPartAButton_clicked(bool check){
    Q_EMIT move_ag2(0.25,0.0,1.45,0,3.1415,(-23.5)*(M_PI/180.0));
    Q_EMIT move_ag2(homeX,homeY2,homeZ,homeRoll,homePitch*(M_PI/180.0),homeYaw);
    double grippingAngle = (160.0+anglePartA)*(M_PI/180.0);
    Q_EMIT move_ag1(0.35,-0.20,1.66,0,3.1415,0.0);
    Q_EMIT move_ag1(partAInTag(0,3),partAInTag(1,3),1.35,0,3.1415,grippingAngle);

    QString angle = "Gripping part A at angle: ";
    angle.append(QString::number(grippingAngle*(180.0/M_PI)));
    printToLog(angle);

    QString partA = "X: ";
    partA.append(QString::number(partAInTag(0,3)));
    partA.append(", Y: ");
    partA.append(QString::number(partAInTag(1,3)));
    partA.append(", Z: ");
    partA.append(QString::number(partAInTag(2,3)));

    printToLog("Position of part A in world:");
    printToLog(partA);

    Q_EMIT openAG1Gripper();
    Q_EMIT move_ag1(partAInTag(0,3),partAInTag(1,3),1.203,0,3.1415,grippingAngle);
    Q_EMIT closeAG1Gripper();
    Q_EMIT move_ag1(partAInTag(0,3),partAInTag(1,3),1.45,0,3.1415,grippingAngle);
}

void MainWindow::on_moveGripperPartBButton_clicked(bool check){
    Q_EMIT move_ag2(homeX,homeY2,homeZ,homeRoll,homePitch*(M_PI/180.0),homeYaw);

    double deployAngle = (160.0+anglePartB+3.5)*(M_PI/180.0);
    Q_EMIT move_ag1(partBInTag(0,3)-0.0023,partBInTag(1,3)-0.0005,1.45,0,3.1415,deployAngle);

    QString angle = "Deploying part A at part B with angle: ";
    angle.append(QString::number(deployAngle*(180.0/M_PI)));
    printToLog(angle);

    QString partB = "X: ";
    partB.append(QString::number(partBInTag(0,3)-0.0023));
    partB.append(", Y: ");
    partB.append(QString::number(partBInTag(1,3)-0.0005));
    partB.append(", Z: ");
    partB.append(QString::number(partBInTag(2,3)));

    printToLog("Position of part B in world");
    printToLog(partB);

    Q_EMIT move_ag1(partBInTag(0,3)-0.0023,partBInTag(1,3)-0.0005,1.375,0,3.1415,deployAngle);
}

void MainWindow::on_assemblePartsButton_clicked(bool check){
    double deployAngle = (160.0+anglePartB+3.5)*(M_PI/180.0);
    Q_EMIT move_ag1(partBInTag(0,3)-0.0023,partBInTag(1,3)-0.0005,1.36,0,3.1415,deployAngle);
}

void MainWindow::on_worldCoordinatesCheckBox_clicked(bool check){
    if(check) {
        if(ui.robotComboBox->currentIndex() == 0) {
            ui.xPosDoubleSpinBox->setValue(homeX);
            ui.yPosDoubleSpinBox->setValue(homeY1);
            ui.zPosDoubleSpinBox->setValue(homeZ);
            ui.rollDoubleSpinBox->setValue(homeRoll);
            ui.pitchDoubleSpinBox->setValue(homePitch);
            ui.yawDoubleSpinBox->setValue(homeYaw);
        } else if(ui.robotComboBox->currentIndex() == 1) {
            ui.xPosDoubleSpinBox->setValue(homeX);
            ui.yPosDoubleSpinBox->setValue(homeY2);
            ui.zPosDoubleSpinBox->setValue(homeZ);
            ui.rollDoubleSpinBox->setValue(homeRoll);
            ui.pitchDoubleSpinBox->setValue(homePitch);
            ui.yawDoubleSpinBox->setValue(homeYaw);
        }
    } else {
        if(ui.robotComboBox->currentIndex() == 0) {
            ui.xPosDoubleSpinBox->setValue(0);
            ui.yPosDoubleSpinBox->setValue(0);
            ui.zPosDoubleSpinBox->setValue(0);
            ui.rollDoubleSpinBox->setValue(0);
            ui.pitchDoubleSpinBox->setValue(0);
            ui.yawDoubleSpinBox->setValue(0);
        } else if(ui.robotComboBox->currentIndex() == 1) {
            ui.xPosDoubleSpinBox->setValue(0);
            ui.yPosDoubleSpinBox->setValue(0);
            ui.zPosDoubleSpinBox->setValue(0);
            ui.rollDoubleSpinBox->setValue(0);
            ui.pitchDoubleSpinBox->setValue(0);
            ui.yawDoubleSpinBox->setValue(0);
        }
    }
}

void MainWindow::on_robotComboBox_currentIndexChanged(int i){
    Q_EMIT on_worldCoordinatesCheckBox_clicked(ui.worldCoordinatesCheckBox->isChecked());
}

void MainWindow::on_resetSequenceButton_clicked(bool check){
    movedToPartA = false;
    movedToPartB = false;
    QString reset = "Reset detection of parts. Start with 3D detection!";
    printToLog(reset);
}

void MainWindow::on_create_training_set_button_clicked(bool check){
    QString msg = "";
    QString partName;
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open STL File"),"/home/minions",tr("STL cad model (*.stl)"));
    if(!filePath.isEmpty()){
        bool ok;
        partName = QInputDialog::getText(this, tr("Name the part"), tr("Part name"), QLineEdit::Normal, tr(""), &ok);
        if(!partName.isEmpty() && ok){
        }
        else{
            return;
        }
    }
    else{
        return;
    }
    msg.append("Rendering file: ");
    msg.append(filePath);
    msg.append(", as partname: ");
    msg.append(partName);
    printToLog(msg);
    pcl::PolygonMesh mesh;
    pcl::io::loadPolygonFileSTL(filePath.toStdString(),mesh);

    pcl::PointCloud<pcl::PointXYZ> scaled_mesh;
    Eigen::Matrix4f scaleMatrix = Eigen::Matrix4f::Identity();
    scaleMatrix(0,0)=0.001f;
    scaleMatrix(1,1)=0.001f;
    scaleMatrix(2,2)=0.001f;

    pcl::fromPCLPointCloud2(mesh.cloud,scaled_mesh);
    pcl::transformPointCloud(scaled_mesh,scaled_mesh,scaleMatrix);
    pcl::toPCLPointCloud2(scaled_mesh, mesh.cloud);

    ModelLoader *render = new ModelLoader(mesh, partName.toStdString());
    render->setCloudResolution(ui.resolution_combobox->currentText().toInt());
    render->setTesselation_level(ui.tesselation_level_combobox->currentIndex()+1);
    render->populateLoader();
}

void MainWindow::disableAuto(){
    ui.autoDetection3dButton->setEnabled(false);
    ui.auto2dFirstPartButton->setEnabled(false);
    ui.auto2dFirstPartAngleButton->setEnabled(false);
    ui.auto2dSecondPartButton->setEnabled(false);
    ui.auto2dSecondPartAngleButton->setEnabled(false);
    ui.moveGripperPartAButton->setEnabled(false);
    ui.moveGripperPartBButton->setEnabled(false);
    ui.assemblePartsButton->setEnabled(false);
    ui.resetSequenceButton->setEnabled(false);
}

void MainWindow::on_loadPCDButton_clicked(bool check){
    runStream = false;
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),"/home/minions",tr("PointCloud (*.pcd)"));
    if(fileName.length() != 0){
        QString tmpstring = "Loading .pcd from ";
        tmpstring.append(fileName);
        printToLog(tmpstring);
        if(ui.loadRgbCheckbox->isChecked()){
            pcl::PointCloud<pcl::PointXYZRGB>::Ptr displayCloud(new pcl::PointCloud<pcl::PointXYZRGB>);
            if(pcl::io::loadPCDFile<pcl::PointXYZRGB>(fileName.toUtf8().constData(), *displayCloud) == -1)
            {
                return;
            }

            displayNewPointCloud(filters->visualize_rgb(displayCloud));
        }
        else{
            pcl::PointCloud<pcl::PointXYZ>::Ptr displayCloud(new pcl::PointCloud<pcl::PointXYZ>);
            if(pcl::io::loadPCDFile<pcl::PointXYZ>(fileName.toUtf8().constData(), *displayCloud) == -1)
            {
                return;
            }
            *currentCloud = *displayCloud;
            displayNewPointCloud(filters->visualize(displayCloud));
        }
    }
}

void MainWindow::on_subscribeToTopicButton_clicked(bool check){
    runStream = true;
    Q_EMIT subscribeToPointCloud2(ui.topicComboBox->currentText());
}

void MainWindow::on_subscribeToTopicButton2_clicked(bool check){
    Q_EMIT subscribeTo2DobjectDetected(ui.topicComboBox2->currentText());
}

void MainWindow::on_detectObjectsButton_clicked(bool check){
    detect3D(ui.objectsListA->currentIndex(),ui.objectsListB->currentIndex());
}

}  // namespace agilus_master_project

