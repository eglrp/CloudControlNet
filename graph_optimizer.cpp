#include "graph_optimizer.h"
#include "find_constraint.h"
#include <glog/logging.h>

using namespace std;
using namespace utility;

void GlobalOptimize::optimizePoseGraph(vector<CloudBlock> &all_blocks, vector<Constraint> &all_cons)
{

	//1.优化器实例化;确定优化方法 such as L-M;
	//2.赋点;赋初值;Fixed Value(基准）[TLS];
	//3.赋边;观测值[配准结果或邻接单位阵];赋边权(信息矩阵;定义鲁棒核函数 such as Huber;
	//4.优化求解;

	LOG(INFO) << "Start to optimize the graph.";

	//set up optimizer
	g2o::SparseOptimizer optimizer;
	auto linearSolver = g2o::make_unique<g2o::LinearSolverEigen<g2o::BlockSolverX::PoseMatrixType>>();
	auto blockSolver = g2o::make_unique<g2o::BlockSolverX>(std::move(linearSolver));
	g2o::OptimizationAlgorithmLevenberg* solver = new g2o::OptimizationAlgorithmLevenberg(std::move(blockSolver));
	optimizer.setAlgorithm(solver);
	optimizer.setVerbose(false);

	//std::vector<g2o::VertexSE3Expmap*> pose_vertices;
	//std::vector<g2o::EdgeSE3Expmap*> edges_smo;  
	//std::vector<g2o::EdgeSE3Expmap*> edges_reg;


	//Set Nodes
	for (int i = 0; i < all_blocks.size(); i++)
	{
		g2o::VertexSE3Expmap* pose_v = new g2o::VertexSE3Expmap();
		pose_v->setId(i);
		if (all_blocks[i].data_type == 2)  pose_v->setFixed(true);   //TLS站位姿变化固定;
		pose_v->setEstimate(g2o::SE3Quat()); //预设值为单位Pose,认为一开始没有位姿变换;
		optimizer.addVertex(pose_v);         //添加结点;
		//pose_vertices.push_back(pose_v); 
	}


	//Set Edges
	for (int j = 0; j < all_cons.size(); j++)
	{
		if (all_cons[j].con_type != 0) //0: A removed constraint  
		{
			g2o::EdgeSE3Expmap* pose_e = new g2o::EdgeSE3Expmap();
			pose_e->setId(j);
			int v0 = all_cons[j].block1.unique_index;
			int v1 = all_cons[j].block2.unique_index;
			pose_e->setVertex(0, optimizer.vertices()[v1]);  // 2 -> 1   //Measurement Trans1_2
			pose_e->setVertex(1, optimizer.vertices()[v0]);
			Eigen::Matrix4d Trans_pose = all_cons[j].Trans1_2.cast<double>();
			pose_e->setMeasurement(g2o::SE3Quat(Trans_pose.topLeftCorner<3, 3>(), Trans_pose.topRightCorner<3, 1>()));  //Set the transformation measurement
			float weight = determineWeight(all_cons[j].block1.data_type, all_cons[j].block2.data_type, all_cons[j].con_type);
			Eigen::Matrix<double, 6, 6> info_mat = Eigen::Matrix<double, 6, 6>::Identity()*weight;  //Set the Information (Weight) Matrix
			pose_e->setInformation(info_mat);
			pose_e->setRobustKernel(new g2o::RobustKernelHuber());  //Set the robust kernel function  
			optimizer.addEdge(pose_e);
			//if (all_cons[j].con_type == 1)      edges_smo.push_back(pose_e);  //Smooth Edges
			//else if (all_cons[j].con_type == 2) edges_reg.push_back(pose_e);  //Registration Edges
		}
	}

	//Optimize
	cout << "Begin Optimization" << endl;
	optimizer.setVerbose(true);
	optimizer.initializeOptimization();
	optimizer.optimize(15); // Number of Iterations
	cout << "End Optimization" << endl;


	//Assign the new pose and output the result
	for (int i = 0; i < all_blocks.size(); i++)
	{
		g2o::VertexSE3Expmap* v_o = dynamic_cast<g2o::VertexSE3Expmap*>(optimizer.vertices()[i]);
		Eigen::Isometry3d pose_o = v_o->estimate();
		all_blocks[i].optimized_pose = pose_o.matrix().cast<float>();
		LOG(INFO) << "Pose index " << i << " (#type " << all_blocks[i].data_type << " , #strip " << all_blocks[i].strip_num << " , #block " << all_blocks[i].num_in_strip << "]";
		LOG(INFO) << all_blocks[i].optimized_pose;
	}

	//To guarentee the robustness, chi2 (卡方） 检验
	/*for (int i = 0; i < iterations.size(); ++i)
	{
	double last_active_chi2 = 0.0, epsilon = 0.000001;
	for (int j = 0; j < iterations[i]; ++j)
	{
	optimizer.initializeOptimization(0);
	optimizer.optimize(1);
	optimizer.computeActiveErrors();

	double active_chi2 = optimizer.activeRobustChi2();
	LOG(INFO) << j << "th active_chi2 is " << active_chi2;
	if (std::isnan(active_chi2))
	{
	LOG(WARNING) << "Essential Graph Optimization " << j << "th iteration chi2 is NAN!";
	bSuccess = false;
	bBreak = true;
	break;
	}
	if (std::isinf(active_chi2))
	{
	LOG(WARNING) << "Essential Graph Optimization " << j << "th iteration chi2 is INF!";
	bSuccess = false;
	bBreak = true;
	break;
	}

	double improvment = last_active_chi2 - active_chi2;
	if (j > 0 && improvment < epsilon) // negative/ not enough improvement
	{
	LOG(WARNING) << "Essential Graph Optimization negative/ not enough improvement(" << improvment << " < " << epsilon << ")";
	bBreak = true;
	break;
	}
	else if (i == 0 && active_chi2 < epsilon) // error is under epsilon
	{
	LOG(INFO) << "Essential Graph Optimization error(" << active_chi2 << ") is under epsilon(" << epsilon << ")";
	bBreak = true;
	break;
	}

	last_active_chi2 = active_chi2;
	}*/

}

float GlobalOptimize::determineWeight(int node0_type, int node1_type, int edge_type)
{
	float weight_result = 1.0; //先设单位权吧;
	if (node0_type == 1 && node1_type == 1 && edge_type == 2) weight_result = 0.1; //ALS+ALS registration
	if ((node0_type == 1 && node1_type == 3) || (node0_type == 3 && node1_type == 1)) weight_result = 0.5; //ALS+MLS registration
	
	if (edge_type == 1) weight_result = 0.5; //ALS+ALS / MLS+MLS smooth


	return weight_result;
}
	





