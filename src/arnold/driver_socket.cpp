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

node_parameters
{
   AiParameterSTR("filename", "");
   AiParameterStr("port", default_port);
   AiParameterStr("host", default_host);

   AiMetaDataSetStr(mds, NULL, "maya.translator", "socket");
   AiMetaDataSetStr(mds, NULL, "maya.attr_prefix", "");
   AiMetaDataSetBool(mds, NULL, "single_layer_driver", true);
   AiMetaDataSetBool(mds, NULL, "display_driver", true);
}

node_initialize
{
   AiMsgInfo("[driver_socket] node_initialize");
   // set second arg to true once multiple outputs are supported
   ShaderData *data = (ShaderData*)AiMalloc(sizeof(ShaderData));
   data->out = NULL;
   AiDriverInitialize(node, false, data);
}

node_update
{
}

driver_supports_pixel_type
{
   switch (pixel_type)
   {
      case AI_TYPE_FLOAT:
      case AI_TYPE_POINT2:
      case AI_TYPE_POINT:
      case AI_TYPE_VECTOR:
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
   ShaderData *data = (ShaderData*)AiDriverGetLocalData(node);
   if (data->out != NULL)
       return;

   std::string filename = AiNodeGetStr(node, "filename");

   AiMsgInfo("[driver_socket] Connecting");
   // assert name ends in .socket so that SocketInput is used on the receiving end
   if (!Strutil::iends_with(filename, ".socket"))
       filename += ".socket";

   ImageOutput* out = v1_1::ImageOutput::create(filename);
   if (!out)
   {
      AiMsgError("[driver_socket] %s", out->geterror().c_str());
      return;
   }

   ImageSpec spec = v1_1::ImageSpec();

   const char *aov = "";
   int pixel_type;

   if (!AiOutputIteratorGetNext (iterator, &aov, &pixel_type, NULL))
      return;

   switch (pixel_type)
   {
      case AI_TYPE_FLOAT:
         spec.nchannels = 1;
         break;
      case AI_TYPE_POINT2:
         spec.nchannels = 2;
         break;
      case AI_TYPE_RGB:
      case AI_TYPE_VECTOR:
      case AI_TYPE_POINT:
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
   spec.attribute("port", AiNodeGetStr(node, "port"));
   spec.attribute("host", AiNodeGetStr(node, "host"));

   if (!out->open(filename, spec))
   {
      AiMsgWarning("[driver_socket] %s", out->geterror().c_str());
   }

   data->out = out;

}

driver_prepare_bucket
{
   AiMsgDebug("[driver_socket.%d] prepare bucket (%d, %d)", tid, bucket_xo, bucket_yo);
}

driver_write_bucket
{
   AiMsgInfo("[driver_socket] write bucket   (%d, %d)",  bucket_xo, bucket_yo);

   int         pixel_type;
   const void* bucket_data;

   // TODO: multiple-aovs
   // we must convert from arnold tiles, where pixels are grouped by aovs
   // to oiio tiles, where aov pixels are interleaved.
   // for now, just get the first AOV layer
   if (!AiOutputIteratorGetNext(iterator, NULL, &pixel_type, &bucket_data))
   {
      AiMsgError("[driver_socket] Could not get first AOV");
      return;
   }
   ShaderData *data = (ShaderData*)AiDriverGetLocalData(node);
   if (!data->out->write_rectangle (bucket_xo, bucket_xo + bucket_size_x -1,
                                    bucket_yo, bucket_yo + bucket_size_y -1,
                                    0, 0, TypeDesc::FLOAT, bucket_data))
   {
      AiMsgError("[driver_socket] %s", data->out->geterror().c_str());
   }
}


driver_close
{
   AiMsgInfo("[driver_socket] driver close");
}

node_finish
{
   AiMsgInfo("[driver_socket] driver finish");
   ShaderData *data = (ShaderData*)AiDriverGetLocalData(node);
   // This line crashes the render, likely because we haven't finished sending data to the server.
   // we should use another thread as in this example: http://www.boost.org/doc/libs/1_38_0/doc/html/boost_asio/example/chat/chat_client.cpp
   data->out->close();
   AiFree(data);
   // release the driver
   AiDriverDestroy(node);
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
