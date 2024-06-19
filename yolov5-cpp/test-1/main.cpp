#include <opencv2/opencv.hpp>
#include <thread>
#include <fstream>
#include <iostream>
#include "sound.h"

class YOLODetector 
{
private:
    cv::dnn::Net net;
    std::vector<std::string> class_list;
    const std::vector<cv::Scalar> colors = {cv::Scalar(255, 255, 0), cv::Scalar(0, 255, 0), cv::Scalar(0, 255, 255), cv::Scalar(255, 0, 0)};
    const float INPUT_WIDTH = 640.0;
    const float INPUT_HEIGHT = 640.0;
    const float SCORE_THRESHOLD = 0.2;
    const float NMS_THRESHOLD = 0.4;
    const float CONFIDENCE_THRESHOLD = 0.6;

    AudioPlayer player;
    const char *file = "/home/hedef-ubuntu/cpp/sound/sound.wav";

    struct Detection 
    {
        int class_id;
        float confidence;
        cv::Rect box;
    };

    cv::Mat format_yolov5(const cv::Mat &source) 
    {
        int col = source.cols;
        int row = source.rows;
        int _max = std::max(col, row);
        cv::Mat result = cv::Mat::zeros(_max, _max, CV_8UC3);
        source.copyTo(result(cv::Rect(0, 0, col, row)));
        return result;
    }

public:
    YOLODetector(const std::string& class_file_path, const std::string& model_path, bool use_cuda) 
    {
        class_list = load_class_list(class_file_path);
        load_net(model_path, use_cuda);

        AudioPlayer_init(&player, (char*)file);
    }

    std::vector<std::string> load_class_list(const std::string& file_path) 
    {
        std::vector<std::string> classes;
        std::ifstream file(file_path);
        std::string line;
        while (std::getline(file, line)) 
        {
            classes.push_back(line);
        }
        return classes;
    }

    void load_net(const std::string& model_path, bool use_cuda) 
    {
        net = cv::dnn::readNet(model_path);
        if (use_cuda) 
        {
            net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA_FP16);
        } 
        else 
        {
            net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        }
    }

    void detect(cv::Mat &image) 
    {
        cv::Mat blob;
        auto input_image = format_yolov5(image);
        cv::dnn::blobFromImage(input_image, blob, 1./255., cv::Size(INPUT_WIDTH, INPUT_HEIGHT), cv::Scalar(), true, false);
        net.setInput(blob);
        std::vector<cv::Mat> outputs;
        net.forward(outputs, net.getUnconnectedOutLayersNames());

        float x_factor = input_image.cols / INPUT_WIDTH;
        float y_factor = input_image.rows / INPUT_HEIGHT;

        float *data = (float *)outputs[0].data;

        const int dimensions = 85;
        const int rows = 25200;

        std::vector<int> class_ids;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;

        for (int i = 0; i < rows; ++i) 
        {
            float confidence = data[4];
            if (confidence >= CONFIDENCE_THRESHOLD) 
            {
                float * classes_scores = data + 5;
                cv::Mat scores(1, class_list.size(), CV_32FC1, classes_scores);
                cv::Point class_id;
                double max_class_score;
                cv::minMaxLoc(scores, 0, &max_class_score, 0, &class_id);
                if (max_class_score > SCORE_THRESHOLD) 
                {
                    confidences.push_back(confidence);
                    class_ids.push_back(class_id.x);
                    float x = data[0];
                    float y = data[1];
                    float w = data[2];
                    float h = data[3];
                    int left = int((x - 0.5 * w) * x_factor);
                    int top = int((y - 0.5 * h) * y_factor);
                    int width = int(w * x_factor);
                    int height = int(h * y_factor);
                    boxes.push_back(cv::Rect(left, top, width, height));
                }
            }
            data += dimensions;
        }

        std::vector<int> nms_result;
        cv::dnn::NMSBoxes(boxes, confidences, SCORE_THRESHOLD, NMS_THRESHOLD, nms_result);
        for (size_t i = 0; i < nms_result.size(); i++) 
        {
            int idx = nms_result[i];
            auto detection = Detection{class_ids[idx], confidences[idx], boxes[idx]};
            draw_detection(image, detection);
        }
    }

    void draw_detection(cv::Mat& image, const Detection& detection) 
    {
        auto box = detection.box;
        auto class_id = detection.class_id;
        const auto color = colors[class_id % colors.size()];
        cv::rectangle(image, box, color, 3);
        cv::rectangle(image, cv::Point(box.x, box.y - 20), cv::Point(box.x + box.width, box.y), color, cv::FILLED);
        cv::putText(image, class_list[class_id].c_str(), cv::Point(box.x, box.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));

        int area = box.width * box.height;
        area = area / 3072;

        std::cout << area << std::endl;
        if (area > 25) 
        {
            std::cout << "tehlike" << std::endl;
            if(!AudioPlayer_initialize(&player)) std::cout << "voice settings error" << std::endl;

            AudioPlayer_play(&player);
            AudioPlayer_destroy(&player);
        }
    }
};

class VideoCaptureWrapper
{
public:
    VideoCaptureWrapper(YOLODetector& detector) : detector(detector), cap(0, cv::CAP_V4L2)
    {
        if (!cap.isOpened())    std::exit(EXIT_FAILURE);
    }

    void start() 
    {
        thread = std::thread([this] { this->captureLoop(); });
    }

    ~VideoCaptureWrapper() 
    {
        if (thread.joinable())  thread.join();

        cap.release();
        cv::destroyAllWindows();
    }

private:
    YOLODetector& detector;
    cv::VideoCapture cap;
    std::thread thread;

    void captureLoop() 
    {
        cv::Mat frame;

        uint8_t frameCount = 0;
        double startTime = cv::getTickCount();

        while (1) 
        {
            cap.read(frame);
            if (frame.empty())  std::exit(1);

            detector.detect(frame);

            cv::imshow("FRAME", frame);

            int key = cv::waitKey(1);

            if (key == 'q' || key == 'Q')  break;

            frameCount++;

            if (frameCount % 30 == 0) 
            {
                double endTime = cv::getTickCount();
                double totalTime = (endTime - startTime) / cv::getTickFrequency(); 
                double fps = 30 / totalTime; 
                std::cout << "FPS: " << fps << std::endl;
                startTime = cv::getTickCount(); 
                frameCount = 0;
            }
        }
    }
};


int main(int argc, char **argv)
{
    const std::string class_file_path = "/home/hedef-ubuntu/cpp/yolov5-opencv-cpp-python/config_files/classes.txt";
    const std::string model_path = "/home/hedef-ubuntu/cpp/yolov5-opencv-cpp-python/config_files/yolov5n.onnx";
    
    bool use_cuda = argc > 1 && strcmp(argv[1], "cuda") == 0;

    YOLODetector detector(class_file_path, model_path, use_cuda);

    VideoCaptureWrapper wrapper(detector);

    wrapper.start();

    return EXIT_SUCCESS;
}
