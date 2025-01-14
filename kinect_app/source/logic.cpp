#include "api.h"
#include "logic.h"
#include "bitmap.h"
#include "trace.h"

#include <jsoncpp/include/jsoncpp.h>

#include <string>
#include <sstream>

#define NUI_OUT_SKEL_POS(d, idx)\
	"[" << d.SkeletonPositions[idx].x << ";" << d.SkeletonPositions[idx].y << ";" << d.SkeletonPositions[idx].z << "]"

namespace kinect_app
{

Engine::Engine()
{
}

Engine::~Engine()
{
	if (!!m_ns.get())
	{
		m_ns->Stop();
	}
}

void Engine::StartNetworkService()
{
	m_ns = network::CreateWebsocketService(7681);

	m_ns->Start(*this);
}

void Engine::ConsumeDepthInput(const NUI_IMAGE_FRAME& frame)
try
{
	static image::Bitmap bitmap(NUI_IMAGE_RESOLUTION_640x480); // @todo: make class member, pass resolutaion as parameter

	bitmap.Write(frame);

	std::vector<BYTE> image; // @todo: could be optimized - no need to allocate every time
	bitmap.Convert(image::image_types::Gif, image);

	std::string out; // @todo: could be optimized - no need to allocate every time
	this->ComposeDepthString(&image[0], image.size(), out);
	{
		ScopedLock<CriticalSection> lock(m_depthGuard);
		m_depthAsString = out;
	}

	m_depthReady.Wake();
}
catch (...)
{}

void Engine::ConsumeSkeletonInput(const NUI_SKELETON_FRAME& frame)
try
{
	std::string out;
	this->ComposeSkeletonString(frame, out);
	{
		ScopedLock<CriticalSection> lock(m_skeletonGuard);
		m_skeletonAsString = out;
	}

	m_skeletonReady.Wake();
}
catch (...)
{}

void Engine::ConsumeColorInput(const NUI_IMAGE_FRAME& frame)
try
{
}
catch (...)
{}

// ServiceCallback
void Engine::OnDataReceived(const char* data, size_t size, network::Response& response)
{
	KINECT_TRACE_DBG("Engine::OnDataReceived");

	std::istringstream stream(
		reinterpret_cast<const char*>(data),
		std::istringstream::in
		);

	Json::Value  root;
	Json::Reader reader;

	if (const bool parsedSuccess = reader.parse(stream, root, false))
	{
		const Json::Value proto  = root["protocol"];
		const Json::Value entity = root["entity"];
				
		const std::string& requestType = entity.asString();

		if (requestType == "skeleton")
		{
			this->OnSkeletonRequest(response);
		}
		else if (requestType == "depth")
		{
			this->OnDepthRequest(response);
		}
	}
	else
	{
		// @todo: handle error
	}

	KINECT_TRACE_DBG("Engine::OnDataReceived finished");
}

void Engine::OnSkeletonRequest(network::Response& response)
{
	KINECT_TRACE_DBG("Engine::OnSkeletonRequest");
	{
		ScopedLock<CriticalSection> lock(m_skeletonGuard);
		
		m_skeletonReady.Sleep(m_skeletonGuard);

		m_skeletonOut = m_skeletonAsString;
	}
	
	KINECT_TRACE_DBG("Engine::OnSkeletonRequest: got skeleton");

	Json::Value root(m_skeletonOut.c_str());
	Json::FastWriter writer;
	const std::string& msg = writer.write(root);

	response.Send(msg.c_str(), msg.size());

	KINECT_TRACE_DBG("Engine::OnSkeletonRequest: response sent");
}

void Engine::OnDepthRequest(network::Response& response)
{
	{
		ScopedLock<CriticalSection> lock(m_skeletonGuard);

		m_depthReady.Sleep(m_skeletonGuard);

		m_depthOut = m_depthAsString;
	}

	Json::Value root(m_depthOut.c_str());
	Json::FastWriter writer;
	const std::string& msg = writer.write(root);
	
	response.Send(msg.c_str(), msg.size());
}
	
void Engine::ComposeDepthString(const BYTE* image, size_t imageSize, std::string& out)
{
	out.clear();
	out.append("Depth:[").append(detail::base64_encode(reinterpret_cast<const unsigned char*>(image), imageSize)).append("]");
}

void Engine::ComposeSkeletonString(const NUI_SKELETON_FRAME& frame, std::string& str)
{
	std::stringstream out;

	for (size_t i = 0 ; i < NUI_SKELETON_COUNT ; ++i)
	{
		const NUI_SKELETON_DATA& skelData = frame.SkeletonData[i];

		if (skelData.eTrackingState == NUI_SKELETON_NOT_TRACKED) // @todo: track skeletons
		{
			continue;
		}

		out << "Skeleton:";
		out << "["
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_HIP_CENTER)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_SPINE)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_SHOULDER_CENTER)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_HEAD)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_SHOULDER_LEFT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_ELBOW_LEFT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_WRIST_LEFT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_HAND_LEFT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_SHOULDER_RIGHT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_ELBOW_RIGHT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_WRIST_RIGHT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_HAND_RIGHT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_HIP_LEFT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_KNEE_LEFT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_ANKLE_LEFT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_FOOT_LEFT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_HIP_RIGHT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_KNEE_RIGHT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_ANKLE_RIGHT)
			<< NUI_OUT_SKEL_POS(skelData, NUI_SKELETON_POSITION_FOOT_RIGHT)
		<< "];";

		break; // @todo: wtf??
	}
	str = out.str();
}

}
