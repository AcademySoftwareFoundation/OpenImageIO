#include <ai.h>
#include <ai_critsec.h>
#include <ai_drivers.h>
#include <ai_filters.h>
#include <ai_msg.h>
#include <ai_render.h>
#include <ai_universe.h>

#include <OpenImageIO/imageio.h>
#include <cstdio>

OIIO_NAMESPACE_USING

AI_DRIVER_NODE_EXPORT_METHODS(SocketDriverMtd);

typedef struct
{
   ImageOutput* out;
} ShaderData;

/*
time_t s_start_time;


class Bucket
{
public:
   Bucket(int xo, int yo) {x=xo; y=yo;}
   int x;
   int y;
   inline bool operator==(const Bucket& other) const { return x == other.x && y == other.y; }
   inline bool operator!=(const Bucket& other) const { return x != other.x || y != other.y; }
   inline bool operator<(const Bucket& other) const
   {
      if (y==other.y)
         return x < other.x;
      else
         return y < other.y;
   }
};


struct COutputDriverData
{
   AtBBox2   refresh_bbox;
   float     gamma;
   unsigned int    imageWidth, imageHeight;
   bool rendering;
};

enum EDisplayUpdateMessageType
{
   MSG_BUCKET_PREPARE,
   MSG_BUCKET_UPDATE,
   MSG_IMAGE_COMPLETE,
   MSG_RENDER_DONE
};

// Do not use copy constructor and assignment operator outside
// of a critical section
// (basically do not use them, CMTBlockingQueue uses them)
struct CDisplayUpdateMessage
{

   EDisplayUpdateMessageType msgType;
   AtBBox2                   bucketRect;
   RV_PIXEL*                 pixels;

   CDisplayUpdateMessage(EDisplayUpdateMessageType msg = MSG_BUCKET_PREPARE,
                           int minx = 0, int miny = 0, int maxx = 0, int maxy = 0,
                           RV_PIXEL* px = NULL)
   {
      msgType         = msg;
      bucketRect.minx = minx;
      bucketRect.miny = miny;
      bucketRect.maxx = maxx;
      bucketRect.maxy = maxy;
      pixels          = px;
   }
};

static CMTBlockingQueue<CDisplayUpdateMessage> s_displayUpdateQueue;
static COutputDriverData                       s_outputDriverData;
static bool                                    s_finishedRendering;
static MString                                 s_camera_name;
static MString                                 s_panel_name;


static std::map<Bucket,int> s_buckets;
*/

node_parameters
{
   AiParameterSTR("socket_filename", "foo?port=10111.socket");
}

node_initialize
{
   // set second arg to true once multiple outputs are supported
   AiDriverInitialize(node, false, AiMalloc(sizeof(ShaderData)));
}

node_update
{
}

driver_supports_pixel_type
{
   switch (pixel_type)
   {
      case AI_TYPE_RGB:
      case AI_TYPE_RGBA:
         return true;
      default:
         return false;
   }
}

driver_extension
{
   return NULL;
}

driver_open
{
   //const char* filename = AiNodeGetStr(node, "socket_filename");
   //const char* filename = "foo?port=10111&host=127.0.0.1.socket";
   const char* filename = "foo.socket";

   AiMsgInfo("[driver_socket] Connecting");
   ImageOutput* out = ImageOutput::create(filename);
   if (!out)
   {
      AiMsgError("[driver_socket] %s", out->geterror().c_str());
      return;
   }

   ImageSpec spec = ImageSpec();

   const char *name = "";
   int pixel_type;

   if (!AiOutputIteratorGetNext (iterator, &name, &pixel_type, NULL))
      return;

   switch (pixel_type)
   {
      case AI_TYPE_RGB:
         spec.nchannels = 3;
         break;
      case AI_TYPE_RGBA:
         spec.nchannels = 4;
         break;
      default:
         AiMsgError("[driver_socket] Unsupported data type");
         return;
   }

   spec.format = TypeDesc::FLOAT;
   spec.x = data_window.minx;
   spec.y = data_window.miny;
   spec.z = 0;
   spec.width = data_window.maxx - data_window.minx + 1;
   spec.height = data_window.maxy - data_window.miny + 1;
   spec.depth = 1;
   spec.full_x = display_window.minx;
   spec.full_y = display_window.miny;
   spec.full_z = 0;
   spec.full_width = display_window.maxx - display_window.minx + 1;
   spec.full_height = display_window.maxy - display_window.miny + 1;
   spec.full_depth = 1;
   spec.tile_width = bucket_size;
   spec.tile_height = bucket_size;

   if (!out->open(filename, spec))
   {
      AiMsgWarning("[driver_socket] %s", out->geterror().c_str());
   }

   ShaderData *data = (ShaderData*)AiNodeGetLocalData(node);
   data->out = out;

}

driver_prepare_bucket
{
   AiMsgDebug("[driver_socket.%d] prepare bucket (%d, %d)", tid, bucket_xo, bucket_yo);

//   ThreadData* pdata = &data[tid];
//   if (!pdata->initialized)
//   {
//      pdata->initialized = true;
//      AiMsgDebug("[driver_socket.%d] Initializing", tid);
//      // char buf[256];
//      // sprintf(buf, "/var/tmp/test.sqlite3.%d", tid);
//      if (init_database("/var/tmp/test.sqlite3", &pdata->db_handle, &pdata->db_stmt))
//         return;
//      // TODO: move this into init_database?
//      pdata->globals_list = new std::vector<AtShaderGlobals>;
//      pdata->node_list = new std::vector<NodeData>;
//   }

   //sql_exec(&pdata->db_handle, "BEGIN;");
   /*
   CDisplayUpdateMessage   msg(MSG_BUCKET_PREPARE,
                               bucket_xo, bucket_yo,
                               bucket_xo + bucket_size_x - 1, bucket_yo + bucket_size_y - 1,
                               NULL) ;

   s_displayUpdateQueue.push(msg);
   */
}

driver_write_bucket
{
   AiMsgInfo("[driver_socket] write bucket   (%d, %d)",  bucket_xo, bucket_yo);
   std::cout << "test" << std::endl;

   int         pixel_type;
   const void* bucket_data;

   // get the first AOV layer
   if (!AiOutputIteratorGetNext(iterator, NULL, &pixel_type, &bucket_data))
   {
      AiMsgError("[driver_socket] Could not get first AOV");
      return;
   }
   ShaderData *data = (ShaderData*)AiNodeGetLocalData(node);
   if (!data->out->write_tile (bucket_xo, bucket_yo, 0,
                               TypeDesc::FLOAT, bucket_data))
   {
      AiMsgError("[driver_socket] %s", data->out->geterror().c_str());
   }

}


driver_close
{
   AiMsgInfo("[driver_socket] driver close");
   ShaderData *data = (ShaderData*)AiNodeGetLocalData(node);
   data->out->close();
}

node_finish
{
   AiMsgInfo("[driver_socket] driver finish");
   // release the driver
   AiDriverDestroy(node);
   ShaderData *data = (ShaderData*)AiNodeGetLocalData(node);
   AiFree(data);
}



node_loader
{
   switch (i)
   {
   case 0:
      node->methods     = SocketDriverMtd;
      node->output_type = AI_TYPE_NONE;
      node->name        = "driver_socket";
      node->node_type   = AI_NODE_DRIVER;
      break;
   default:
      return false;
   }
   sprintf(node->version, AI_VERSION);

   return true;
}
