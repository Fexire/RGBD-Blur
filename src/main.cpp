#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>
#include <string>

/* Load image with specific flag */
int load_image(const std::string name, cv::Mat &image, int flags)
{
  image = cv::imread(name, flags);
  if (image.empty())
  {
    std::cout << "error loading " << name << std::endl;
    return -1;
  }
  return 0;
}

/* Create an odd gaussian kernel*/
cv::Mat gaussian_blur_kernel(int size, float sigma)
{
  int N = size;
  // if size is even
  if (N % 2 == 0)
  {
    N++;
  }
  // initialize kernel
  cv::Mat kernel(N, N, CV_32F);
  // for kernel normalization
  float sum = 0;
  // each rows
  for (int i = 0; i < kernel.rows; i++)
  {
    // each columns
    for (int j = 0; j < kernel.cols; j++)
    {
      //center position
      float position = (-(N - 1) * 0.5);
      // get value from i - N/2 to i + N/2 to center gaussian distribution (same for j)
      float squareDist = pow(position + i, 2) + pow(position + j, 2);
      // apply gaussian formula
      kernel.at<float>(i, j) = exp(-squareDist / (2.0 * sigma * sigma));
      // increment sum
      sum += kernel.at<float>(i, j);
    }
  }
  // normalize kernel
  kernel /= sum;

  return kernel;
}

/* mapping which assign specific gaussian kernel to values from 0 to 255 */
std::array<cv::Mat, 256> gaussian_mapping(int focus, float size)
{
  // initialize array of size 256
  std::array<cv::Mat, 256> hist;
  // gap for depth incrementation
  float gap = size / std::max(256 - focus, focus);
  for (int i = 0; i < focus; i++) //high blur to low blur -> back to focus
  {
    // kernel size
    float new_size = (focus - i) * gap;
    // assign new kernel
    hist[i] = gaussian_blur_kernel(int(new_size + 1), new_size / 2.);
  }
  for (int i = focus; i < 256; i++) //low blur to high blur -> focus to front
  {
    float new_size = (i - focus + 1) * gap;
    hist[i] = gaussian_blur_kernel(int(new_size + 1), new_size / 2.);
  }
  return hist;
}

/* apply convolution depending on RBGD properties */
void filter(const cv::Mat &src, const cv::Mat &src_depth,
            cv::Mat &dst, int focus)
{
  // get gaussian_mapping for specific focus and image size
  auto hist = gaussian_mapping(focus, std::max(src.rows, src.cols) / 60.);
  // initialize result image
  dst = cv::Mat(src.rows, src.cols, CV_32FC3, cv::Scalar(0., 0., 0.));
  // for each rows
  for (int x = 0; x < src.rows; x++)
  {
    // for each columns
    for (int y = 0; y < src.cols; y++)
    {
      // get depth of target pixel
      uchar current_depth = src_depth.at<uchar>(x, y);
      // get kernel for the target pixel
      cv::Mat kernel = hist[current_depth];
      // get kernel size
      int N2 = kernel.cols / 2;
      // sum for pixel guessing
      float sum = 0.;
      // for each kernel rows
      for (int i = x - N2; i <= x + N2; i++)
      {
        // for each kernel columns
        for (int j = y - N2; j <= y + N2; j++)
        {
          // if pixel is not out of image
          if (i > 0 && i < src.rows && j > 0 && j < src.cols)
          {
            // if pixel pointed by kernel is behind target pixel
            if (src_depth.at<uchar>(i, j) <= current_depth + 20)
            {
              dst.at<cv::Vec3f>(x, y) += src.at<cv::Vec3f>(i, j) * kernel.at<float>(i - x + N2, j - y + N2);
            }
            // else pixel is ahead target pixel
            else
            {
              sum += kernel.at<float>(i - x + N2, j - y + N2);
            }
          }
          // else pixel is out of image
          else
          {
            sum += kernel.at<float>(i - x + N2, j - y + N2);
          }
        }
      }
      // if sum is greater than 0 we add dst value to normalize pixel
      dst.at<cv::Vec3f>(x, y) += dst.at<cv::Vec3f>(x, y) * (sum / (1. - sum));
    }
  }
}

// images container
struct Data
{
  cv::Mat image;
  cv::Mat image_depth;
  cv::Mat blured_image;
};

/* function call on click */
void callBackKeyboard(int event, int x, int y, int flags, void *userdata)
{
  // cast data
  Data *data = (Data *)userdata;
  switch (event)
  {
  // left click
  case cv::EVENT_LBUTTONDOWN:
    // filter image depending on click focus
    filter(data->image, data->image_depth, data->blured_image, data->image_depth.at<uint8_t>(y, x));
    break;
  case cv::EVENT_RBUTTONDOWN:
  case cv::EVENT_MBUTTONDOWN:
  case cv::EVENT_MOUSEMOVE:
    break;
  }
}

int main(int argc, char **argv)
{
  // program usage
  if (argc != 3)
  {
    std::cout << "usage: " << argv[0] << " image image-depth" << std::endl;
    return -1;
  }

  // initialize data
  Data data;

  //load RGB image
  if (load_image(argv[1], data.image, 1) == -1)
  {
    return -1;
  }
  // load depth image as GRAYSCALE image
  if (load_image(argv[2], data.image_depth, cv::IMREAD_GRAYSCALE) == -1)
  {
    return -1;
  }

  cv::Mat float_image;
  //convert image from unsigned char to float
  data.image.convertTo(float_image, CV_32FC3, 1. / 255.);
  data.image = float_image;
  data.blured_image = data.image;

  //window name
  std::string window_name = "RGBD image";
  /* Create window */
  cv::namedWindow(window_name);
  /* set callback function */
  cv::setMouseCallback(window_name, callBackKeyboard, &data);

  bool quit = false;
  while (!quit) // while 'q' is not pressed
  {
    // show blured image in "RGBD image" window
    cv::imshow(window_name, data.blured_image);
    int key = cv::waitKey(500) % 256;
    if (key == 27 || key == 'q')
      quit = true;
  }

  return 0;
}
