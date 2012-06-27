#include <XnCppWrapper.h> 
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "opennpy_aux.h"


#define CHECK_RC(rc, what)					       \
	if (rc != XN_STATUS_OK)					       \
	{							       \
		printf("%s failed: %s\n", what, xnGetStatusString(rc));\
		return rc;					       \
	}

xn::Context context;

std::vector<xn::DepthGenerator> depthGens;
std::vector<xn::ImageGenerator> imageGens;
std::vector<xn::DepthMetaData*> depthDatas;
std::vector<xn::ImageMetaData*> imageDatas;
//xn::NodeInfoList device_node_info_list;
xn::NodeInfoList depth_node_info_list;
xn::NodeInfoList image_node_info_list;

int initialized = 0;

int opennpy_init(void)
{
    XnStatus nRetVal = XN_STATUS_OK;
    nRetVal = context.Init();
    CHECK_RC(nRetVal, "Initialize context");

    XnMapOutputMode mapMode; 
    mapMode.nXRes = 640;
    mapMode.nYRes = 480;
    mapMode.nFPS = 30; 

    // Initialize all the depth image generators
    context.EnumerateProductionTrees(XN_NODE_TYPE_DEPTH, NULL, 
                                     depth_node_info_list);
    CHECK_RC(nRetVal, "Enumerate depth generators");
    for (xn::NodeInfoList::Iterator nodeIt = depth_node_info_list.Begin(); 
         nodeIt != depth_node_info_list.End(); ++nodeIt) {
        xn::NodeInfo node = *nodeIt;

        nRetVal = context.CreateProductionTree(node);
        CHECK_RC(nRetVal, "Create depth generator");

        xn::DepthGenerator gen;
        nRetVal = node.GetInstance(gen);
        CHECK_RC(nRetVal, "Get depth generator instance");

        nRetVal = gen.SetMapOutputMode(mapMode);
        CHECK_RC(nRetVal, "Set depth generator mapmode");
        depthGens.push_back(gen);
        depthDatas.push_back(new xn::DepthMetaData);
    }

    // Initialize all the RGB image generators
    context.EnumerateProductionTrees(XN_NODE_TYPE_IMAGE, NULL, 
                                     image_node_info_list);
    CHECK_RC(nRetVal, "Enumerate image generators");
    for (xn::NodeInfoList::Iterator nodeIt = image_node_info_list.Begin(); 
         nodeIt != image_node_info_list.End(); ++nodeIt) {
        xn::NodeInfo node = *nodeIt;

        nRetVal = context.CreateProductionTree(node);
        CHECK_RC(nRetVal, "Create image generator");

        xn::ImageGenerator gen;
        nRetVal = node.GetInstance(gen);
        CHECK_RC(nRetVal, "Get image generator instance");

        nRetVal = gen.SetMapOutputMode(mapMode);
        CHECK_RC(nRetVal, "Set image generator mapmode");
        imageGens.push_back(gen);
        imageDatas.push_back(new xn::ImageMetaData);
    }

    // In a typical example program, a call to StartGeneratingAll() would
    // go here. But we'll enable the individual streams when we ask for them
    //nRetVal = context.StartGeneratingAll();
    //CHECK_RC(nRetVal, "StartGeneratingAll");

    initialized = 1;
    return 0;
}

uint8_t *opennpy_sync_get_video(int i)
{
    if (!initialized)
        opennpy_init();
    imageGens[i].StartGenerating();
    imageGens[i].GetMetaData(*imageDatas[i]);
    return (uint8_t *)imageDatas[i]->Data();
}

uint16_t *opennpy_sync_get_depth(int i)
{
    if (!initialized)
        opennpy_init();
    depthGens[i].StartGenerating();
    depthGens[i].GetMetaData(*depthDatas[i]);
    return (uint16_t *)depthDatas[i]->Data();
}

void opennpy_sync_update(void)
{
    context.WaitAnyUpdateAll();
}

void opennpy_shutdown(void) {
    context.Release();
    initialized = 0;
}

void opennpy_align_depth_to_rgb(void) {
    if (!initialized)
        opennpy_init();
    for (unsigned int i = 0; i < depthGens.size(); i++) {
        depthGens[i].GetAlternativeViewPointCap().SetViewPoint(imageGens[i]);
    }
}

void estimate_calibration() {
    XnPoint3D p;

    p.X = 0; p.Y = 0; p.Z = -1;
    depthGens[0].ConvertRealWorldToProjective(1, &p, &p);
    double cx = p.X;
    double cy = p.Y;  
    printf("cx:%.3lf, cy:%.3lf\n", cx, cy);

    p.X = 1; p.Y = 1; p.Z = -1;
    depthGens[0].ConvertRealWorldToProjective(1, &p, &p);

    double fx = -(p.X-cx);
    double fy = p.Y-cy;
    printf("fx:%.3lf, fy:%.3lf\n", fx, fy);
}


int opennpy_test(void) { 
    if (!initialized)
	opennpy_init();

    estimate_calibration();
    opennpy_align_depth_to_rgb();
    estimate_calibration();

    for (int i = 0; i < 5; i++) {
        unsigned short *depth_ptr = opennpy_sync_get_depth(0);
        unsigned char *rgb_ptr = opennpy_sync_get_video(0);
        FILE * fp = fopen("out.pgm", "w");
	fprintf(fp, "P5 %d %d 65535\n", 640, 480);
        fwrite(depth_ptr, 640*480*2, 1, fp);
        fclose(fp);
        fp = fopen("out.ppm", "w");
	fprintf(fp, "P6 %d %d 255\n", 640, 480);
        fwrite(rgb_ptr, 640*480*3, 1, fp);
        fclose(fp);
        printf("Here\n");
        opennpy_align_depth_to_rgb();
    }
    // context.StopGeneratingAll(); ... does not work sometimes for me, so just kill it :-/ 
 
    return 0;
} 

