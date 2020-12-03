#include <iostream>
#include "gdal.h"
#include "ogrsf_frmts.h"
#include "boost/geometry/geometry.hpp"
#include <fstream>
#include <vector>
#include <string>

using namespace std;

typedef boost::geometry::model::point<double, 2, boost::geometry::cs::cartesian> pointNew;
typedef boost::geometry::model::box<pointNew> boxNew;
typedef boost::geometry::model::polygon<pointNew> polygonNew;
typedef pair<boxNew, int> pairNew;

/// <summary>
/// Сортировка По возрастанию id
/// </summary>
/// <param name="a">Первый элемент R-дерева</param>
/// <param name="b">Второй элемент R-дерева</param>
/// <returns>Результат сравнения</returns>
bool sortId(const pair<boxNew, int>& a, const pair<boxNew, int>& b)
{
	return (a.second < b.second);
}

/// <summary>
/// Переводит OGRPolygon в boost::geometry::model::polygon
/// </summary>
/// <param name="polygon">OGR- полигон</param>
/// <returns>Boost-полигон</returns>
polygonNew Polygon(OGRPolygon* polygon) {
	auto* exteriorRing = polygon->getExteriorRing();
	polygonNew polygonRight;
	vector<pointNew> points;
	for (auto&& point : exteriorRing)
		boost::geometry::append(polygonRight.outer(), pointNew(point.getX(), point.getY()));
	return polygonRight;
}

void InsertToRTree(GDALDataset* dataset, boost::geometry::index::rtree<pairNew, boost::geometry::index::quadratic<8U, 4U>>& Rtree);

void ReadFromFile(const std::string& test, double& xMin, double& yMin, double& xMax, double& yMax);

void WriteToFile(const std::string& answer, std::vector<pairNew>& result);

/// <summary>
/// Основная программа
/// </summary>
/// <param name="args"></param>
/// <param name="argv"></param>
/// <returns></returns>
int main(int args, char* argv[]) {

	GDALAllRegister();
	GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpenEx(
		R"(data)",
		GDAL_OF_VECTOR,
		nullptr, nullptr, nullptr));
	if (dataset == nullptr) {
		cout << "Cannot open file!" << endl;
		return -1;
	}
	const string test = argv[2];
	const string answer = argv[3];
	double xMin, yMin, xMax, yMax;
	ReadFromFile(test, xMin, yMin, xMax, yMax); //чтение из файла
	boxNew boxMain(pointNew(xMin, yMin), pointNew(xMax, yMax)); //создание основного прямоугольника
	boost::geometry::index::rtree<pairNew, boost::geometry::index::quadratic<8, 4>> Rtree;
	InsertToRTree(dataset, Rtree); //вставка в R-дерево
	vector<pairNew> result;
	Rtree.query(boost::geometry::index::intersects(boxMain), back_inserter(result)); //поиск пересечений
	sort(result.begin(), result.end(), sortId); //сортировка по возрастанию
	WriteToFile(answer, result); //вывод результата
	return 0;
}

/// <summary>
/// Запись данных в файл
/// </summary>
/// <param name="answer">Путь к ответу</param>
/// <param name="result">Результат пересечения с основным прямоугольником</param>
void WriteToFile(const std::string& answer, std::vector<pairNew>& result)
{
	fstream out(answer, ios::out);
	for (int i = 0; i < result.size(); i++)
		out << result[i].second << "\n";
	out.close();
}

/// <summary>
/// Чтение данных из файла
/// </summary>
/// <param name="test">Путь до теста</param>
/// <param name="xMin">Минимальное значение абсциссы</param>
/// <param name="yMin">Минимальное значение ординаты</param>
/// <param name="xMax">Максимальное значение абсциссы</param>
/// <param name="yMax">Максимальное значение ординаты</param>
void ReadFromFile(const std::string& test, double& xMin, double& yMin, double& xMax, double& yMax)
{
	ifstream fin(test);
	if (!fin.is_open()) {
		throw runtime_error("IO Exception");
	}
	fin >> xMin >> yMin >> xMax >> yMax;
	fin.close();
}

/// <summary>
/// Вставка в R-дерево
/// </summary>
/// <param name="dataset">Набор данных</param>
/// <param name="Rtree">R-дерево</param>
void InsertToRTree(GDALDataset* dataset, boost::geometry::index::rtree<pairNew, boost::geometry::index::quadratic<8U, 4U>>& Rtree)
{
	for (auto&& layer : dataset->GetLayers()) {
		for (auto&& feature : layer) {
			auto* geometry = feature->GetGeometryRef();
			boxNew box;
			switch (auto geometryType = geometry->getGeometryType()) {
			case wkbPolygon: {
				polygonNew polygon = Polygon(geometry->toPolygon());
				boost::geometry::envelope(polygon, box);
				break;
			}
			case wkbMultiPolygon: {
				auto* multiPoligon = geometry->toMultiPolygon();
				int i = 1;
				for (auto&& poligon : multiPoligon) {
					polygonNew polygon = Polygon(poligon);
					boost::geometry::envelope(polygon, box);
				}
				break;
			}
			}
			Rtree.insert(pairNew(box, feature->GetFieldAsInteger(0)));
		}
	}
}
