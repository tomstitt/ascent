//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) Lawrence Livermore National Security, LLC and other Ascent
// Project developers. See top-level LICENSE AND COPYRIGHT files for dates and
// other details. No copyright assignment is required to contribute to Ascent.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

#include <energy_engine.hpp>
#include <rover_exceptions.hpp>
#include <utils/rover_logging.hpp>
#include <vtkm/cont/DefaultTypes.h>

namespace rover {

struct ArraySizeFunctor
{
  vtkm::Id  *m_size;
  ArraySizeFunctor(vtkm::Id *size)
   : m_size(size)
  {}

  template<typename T, typename Storage>
  void operator()(const vtkm::cont::ArrayHandle<T, Storage> &array) const
  {
    *m_size = array.GetNumberOfValues();
  } //operator
};

EnergyEngine::EnergyEngine()
  : m_unit_scalar(1.f)
{
  m_tracer = NULL;
}

EnergyEngine::~EnergyEngine()
{
  if(m_tracer) delete m_tracer;
}

void
EnergyEngine::set_data_set(vtkm::cont::DataSet &dataset)
{
  ROVER_INFO("Energy Engine settting data set");
  if(m_tracer) delete m_tracer;

  m_tracer = new vtkm::rendering::ConnectivityProxy(dataset, "");
  m_tracer->SetRenderMode(vtkm::rendering::ConnectivityProxy::RenderMode::Energy);
  m_data_set = dataset;

}

int
EnergyEngine::get_num_channels()
{
  return detect_num_bins();
}


void
EnergyEngine::set_primary_field(const std::string &primary_field)
{
  ROVER_INFO("Energy Engine setting primary field "<<primary_field);
  m_primary_field = primary_field;
  m_tracer->SetScalarField(this->m_primary_field);
}

void
EnergyEngine::set_secondary_field(const std::string &field)
{
  m_secondary_field = field;
  ROVER_INFO("Energy Engine setting secondary field "<<field);
  if(m_secondary_field != "")
  {
    m_tracer->SetEmissionField(this->m_secondary_field);
  }
}

template<typename Precision>
void
EnergyEngine::init_emission(vtkm::rendering::raytracing::Ray<Precision> &rays,
                            const int num_bins)
{
  if(m_secondary_field == "")
  {
    return;
  }
  rays.AddBuffer(num_bins, "emission");
  rays.GetBuffer("emission").InitConst(0);
}

PartialVector32
EnergyEngine::partial_trace(Ray32 &rays)
{
  if(m_tracer == NULL)
  {
    std::cout<<"Engery Engine Error: must set the data set before tracing\n";
  }

  if(this->m_primary_field == "")
  {
    throw RoverException("Energy Engine : primary field is not set. Unable to render\n");
  }

  ROVER_INFO("Energy Engine trace32");

  init_rays(rays);

  m_tracer->SetUnitScalar(m_unit_scalar);
  m_tracer->SetRenderMode(vtkm::rendering::ConnectivityProxy::RenderMode::Energy);
  m_tracer->SetColorMap(m_color_map);
  return m_tracer->PartialTrace(rays);

}

void
EnergyEngine::set_unit_scalar(vtkm::Float32 unit_scalar)
{
    ROVER_INFO("Energy Engine setting unit scalar "<<unit_scalar);
    m_unit_scalar = unit_scalar;
}

void
EnergyEngine::init_rays(Ray32 &rays)
{

  int num_bins = detect_num_bins();
  rays.Buffers.at(0).SetNumChannels(num_bins);
  rays.Buffers.at(0).InitConst(1.);
  init_emission(rays, num_bins);
}

void
EnergyEngine::init_rays(Ray64 &rays)
{

  int num_bins = detect_num_bins();
  rays.Buffers.at(0).SetNumChannels(num_bins);
  rays.Buffers.at(0).InitConst(1.);
  init_emission(rays, num_bins);
}

PartialVector64
EnergyEngine::partial_trace(Ray64 &rays)
{
  if(m_tracer == NULL)
  {
    ROVER_ERROR("energy engine: tracer is NULL data set was never set.");
  }

  if(this->m_primary_field == "")
  {
    throw RoverException("Energy Engine : primary field is not set. Unable to render\n");
  }

  ROVER_INFO("Energy Engine trace64");
  init_rays(rays);

  m_tracer->SetUnitScalar(m_unit_scalar);
  m_tracer->SetRenderMode(vtkm::rendering::ConnectivityProxy::RenderMode::Energy);
  m_tracer->SetColorMap(m_color_map);
  ROVER_INFO("Energy Engine tracing");
  return m_tracer->PartialTrace(rays);
}

int
EnergyEngine::detect_num_bins()
{
  vtkm::Id absorption_size = 0;
  ArraySizeFunctor functor(&absorption_size);
  m_data_set.GetField(this->m_primary_field).
                            GetData().
                            CastAndCallForTypes<vtkm::TypeListAll,
                                                VTKM_DEFAULT_STORAGE_LIST>(functor);
  vtkm::Id num_cells = m_data_set.GetCellSet().GetNumberOfCells();

  assert(num_cells > 0);
  assert(absorption_size > 0);
  if(num_cells == 0)
  {
    ROVER_ERROR("detect bins failed. num cells is 0"
                <<"\n        num cells "<<num_cells
                <<"\n        field size "<<absorption_size);
    m_data_set.PrintSummary(std::cerr);
    throw RoverException("Failed to detect bins. Num cells cannot be 0\n");
  }
  vtkm::Id modulo = absorption_size % num_cells;
  if(modulo != 0)
  {
    ROVER_ERROR("Absoption does not evenly divided the number of cells. Mod = "<<modulo
                <<"\n        num cells "<<num_cells
                <<"\n        field size "<<absorption_size);

    throw RoverException("absorption field size invalid (Is not evenly divided by number of cells\n");
  }
  vtkm::Id num_bins = absorption_size / num_cells;
  ROVER_INFO("Detected "<<num_bins<<" bins ");
  return static_cast<int>(num_bins);
}

vtkmRange
EnergyEngine::get_primary_range()
{
  if(m_tracer == NULL)
  {
    ROVER_ERROR("energy engine: tracer is NULL data set was never set.");
  }
  return m_tracer->GetScalarFieldRange();
}

void
EnergyEngine::set_composite_background(bool on)
{
  if(m_tracer == NULL)
  {
    ROVER_ERROR("energy engine: tracer is NULL data set was never set.");
  }
  m_tracer->SetCompositeBackground(on);
};

void
EnergyEngine::set_primary_range(const vtkmRange &range)
{
  if(m_tracer == NULL)
  {
    ROVER_ERROR("energy engine: tracer is NULL data set was never set.");
  }
  return m_tracer->SetScalarRange(range);
}

}; //namespace rover
