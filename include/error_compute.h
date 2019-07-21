
#ifndef LIDAR_SLAM_ERROR_COMPUTE_H
#define LIDAR_SLAM_ERROR_COMPUTE_H

#include <Eigen/Core>
#include <vector>
#include <iostream>

struct errors {
    double trans_error;
    double rot_error;
    double speed;
    int lenid;

    errors(double trans_error, double rot_error, double speed, int lenid) :
            trans_error(trans_error), rot_error(rot_error), speed(speed), lenid(lenid) {
	}
};

class ErrorCompute {
public:
    int num_lengths = 8;
    //double lengths[10] = {100, 200, 300, 400, 500, 600, 700, 800};
    double lengths[10] = {30, 60, 90, 120, 150, 180, 210, 240};
    //double lengths[10] = {50, 100, 150, 200, 250, 300, 350, 400};
    double ATE = 0; //average translation error
    double ARE = 0; //average rotation error
    double ATE_table[10];
    double ARE_table[10];
    double speed_table[10];

    inline double trans_error_compute(Eigen::Matrix4d &error_mat) {
        return error_mat.block<3, 1>(0, 3).norm();
    }

    inline double rot_error_compute(Eigen::Matrix4d &error_mat) {
        double tmp_angle = 0.5 * (error_mat(0, 0) + error_mat(1, 1) + error_mat(2, 2) - 1.0); //rodrigues's Formula
        return acos(std::max(std::min(tmp_angle, 1.0), -1.0)) * 180 / M_PI;
    }

    void compute_t(const std::vector<Eigen::Matrix4d> &true_pose_vec,
                   const std::vector<Eigen::Matrix4d> &result_pose_vec)
    {
        std::vector<double> dist_sum;
        dist_sum.push_back(0.);
        std::vector<errors> error_vec;
        int len = (int) std::min(result_pose_vec.size(), true_pose_vec.size());
        for (int i = 1; i < len; i++) {
            double tmp_dis = (true_pose_vec[i].block<3, 1>(0, 3) - true_pose_vec[i - 1].block<3, 1>(0, 3)).norm();
            dist_sum.push_back(tmp_dis + dist_sum.back());
        }

        for (int start_flame = 0; start_flame < len; start_flame++) {
            int end_flame = start_flame + 1;
            for (int i = 0; i < num_lengths; i++) {
                while (1) {
                    double distance = dist_sum[end_flame] - dist_sum[start_flame];
                    if (distance < lengths[i]) {
                        end_flame++;
                        if (end_flame >= len) break;
                    } 
                    else {
                        Eigen::Vector3d delta_truth = true_pose_vec[end_flame].block<3, 1>(0, 3) - true_pose_vec[start_flame].block<3, 1>(0, 3);
                        Eigen::Vector3d delta_odom = result_pose_vec[end_flame].block<3, 1>(0, 3) - result_pose_vec[start_flame].block<3, 1>(0, 3);
                        Eigen::Vector3d tmp_trans_error = delta_odom - delta_truth;

						//printf("DEBUG_BJ: trans_error< %f %f %f >\n",	tmp_trans_error[0], tmp_trans_error[1], tmp_trans_error[2]);

                        double speed = (dist_sum[end_flame] - dist_sum[start_flame]) / (0.1 * (end_flame - start_flame));
                        error_vec.push_back(errors(tmp_trans_error.norm() / distance, 0., speed, i));
                        break;
                    }
                }
            }
        }

        std::vector<double> trans_error_table(10), rot_error_table(10), speed_error_table(20);
        double ave_trans_error = 0, ave_rot_error = 0;
        std::vector<int> part_error_num(10), speed_error_num(20);
        int error_num = (int) error_vec.size();
        for (int i = 0; i < error_num; i++) {
            int tmpid = error_vec[i].lenid;
            trans_error_table[tmpid] += error_vec[i].trans_error;
            part_error_num[tmpid]++;
            rot_error_table[tmpid] += error_vec[i].rot_error;
            ave_trans_error += error_vec[i].trans_error;
            ave_rot_error += error_vec[i].rot_error;
            int speedid = (int) (error_vec[i].speed / 3.0);
            if (speedid < 10) {
                speed_error_num[speedid]++;
                speed_error_table[speedid] += error_vec[i].trans_error;
            }
        }

        ATE = ave_trans_error / error_num;
        ARE = ave_rot_error / error_num;
        for (int i = 0; i < num_lengths; i++) {
            if (part_error_num[i] > 0) {
                ATE_table[i] = trans_error_table[i] / part_error_num[i];
                ARE_table[i] = rot_error_table[i] / part_error_num[i];
            }
        }
        for (int i = 0; i < num_lengths; i++) {
            if (speed_error_num[i] > 0) {
                speed_table[i] = speed_error_table[i] / speed_error_num[i];
            }
        }
    }

    void compute(const std::vector<Eigen::Matrix4d> &true_pose_vec,
                 const std::vector<Eigen::Matrix4d> &result_pose_vec) {
        std::vector<double> dist_sum;
        dist_sum.push_back(0.);
        std::vector<errors> error_vec;
        int len = (int) std::min(result_pose_vec.size(), true_pose_vec.size());
        for (int i = 1; i < len; i++) {
            double tmp_dis = (true_pose_vec[i].block<3, 1>(0, 3) - true_pose_vec[i - 1].block<3, 1>(0, 3)).norm();
//            dist_sum.push_back(tmp_dis + dist_sum.back());
            dist_sum.push_back(tmp_dis + dist_sum[dist_sum.size() - 1]);
		}
        for (int start_flame = 0; start_flame < len; start_flame++) {
            int end_flame = start_flame + 1;
            for (int i = 0; i < num_lengths; i++) {
                while (1) {
                    double distance = dist_sum[end_flame] - dist_sum[start_flame];
                    if (end_flame >= len) break;
                    if (distance < lengths[i]) {
                        end_flame++;
                    } else {
                        Eigen::Matrix4d delta_truth = true_pose_vec[start_flame].inverse() * true_pose_vec[end_flame];
                        Eigen::Matrix4d delta_odom = result_pose_vec[start_flame].inverse() * result_pose_vec[end_flame];
                        Eigen::Matrix4d error_mat = delta_truth.inverse() * delta_odom;
                        double speed = (dist_sum[end_flame] - dist_sum[start_flame]) / (0.1 * (end_flame - start_flame));
                        error_vec.push_back(errors(trans_error_compute(error_mat) / distance,
                                                   rot_error_compute(error_mat) / distance, speed, i));
                        break;
                    }
                }
            }
        }

#if 1
        std::vector<double> trans_error_table(10), rot_error_table(10), speed_error_table(20);
        double ave_trans_error = 0, ave_rot_error = 0;
        std::vector<int> part_error_num(10), speed_error_num(20);
        int error_num = (int) error_vec.size();
        for (int i = 0; i < error_num; i++) {
            int tmpid = error_vec[i].lenid;
            trans_error_table[tmpid] += error_vec[i].trans_error;
            part_error_num[tmpid]++;
            rot_error_table[tmpid] += error_vec[i].rot_error;
            ave_trans_error += error_vec[i].trans_error;
            ave_rot_error += error_vec[i].rot_error;
//            int speedid = (int) (error_vec[i].speed / 3.0);
//            if (speedid < 10) {
//                speed_error_num[speedid]++;
//                speed_error_table[speedid] += error_vec[i].trans_error;
//            }
//            std::cout << tmpid << std::endl;
//            std::cout <<  error_vec[i].trans_error << std::endl;
//            std::cout <<  error_vec[i].rot_error << std::endl;
        }
#endif
		ATE = ave_trans_error / error_num;
        ARE = ave_rot_error / error_num;
        for (int i = 0; i < num_lengths; i++) {
            if (part_error_num[i] > 0) {
                ATE_table[i] = trans_error_table[i] / part_error_num[i];
                ARE_table[i] = rot_error_table[i] / part_error_num[i];
            }
        }
//        for (int i = 0; i < num_lengths; i++) {
//            if (speed_error_num[i] > 0) {
//                speed_table[i] = speed_error_table[i] / speed_error_num[i];
//            }
//        }
    }

    void compute(const std::vector<Eigen::Matrix4f> &true_pose_vec,
                 const std::vector<Eigen::Matrix4f> &result_pose_vec) {
        std::vector<Eigen::Matrix4d> true_pose_d, result_pose_d;
        for (int i = 0; i < true_pose_vec.size(); i++) {
            true_pose_d.push_back(true_pose_vec[i].cast<double>());
            result_pose_d.push_back(result_pose_vec[i].cast<double>());
        }
        compute(true_pose_d, result_pose_d);
	}

    void print_error() {
        std::cout << "average translation error: " << ATE * 100 << " %" << std::endl;
        std::cout << "average rotation error: " << ARE << " deg/m" << std::endl;
        for (int i = 0; i < 8; i++) {
            //Test for one submap
            // std::cout << "distance = " << (i + 1) * 20 << "m, trans error = " << ATE_table[i] * 100 <<
            //           " %, rot error = " << ARE_table[i] << " deg/m" << std::endl;
            //Test for full transaction
            std::cout << "distance = " << (i + 1) * 30 << "m, trans error = " << ATE_table[i] * 100 <<
                      " %, rot error = " << ARE_table[i] << " deg/m" << std::endl;
        }
        // for (int i = 0; i < 10; i++) {
        //     std::cout << "speed = " << (i * 3) << "m/s, trans error = " << speed_table[i] << std::endl;
        // }
    }

};

#endif  // LIDAR_SLAM_ERROR_COMPUTE_H