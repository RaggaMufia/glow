// Copyright 2017 Facebook Inc.  All Rights Reserved.
#include "InstrBuilder.h"

#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " output.h output.cpp output.def\n";
    return -1;
  }

  std::cout << "Writing instr descriptors to:\n\t" << argv[1] << "\n\t"
            << argv[2] << "\n\t" << argv[3] << "\n";

  std::ofstream hFile(argv[1]);
  std::ofstream cFile(argv[2]);
  std::ofstream dFile(argv[3]);

  Builder BB(hFile, cFile, dFile);

  //===--------------------------------------------------------------------===//
  //               Memory / Buffer Management
  //===--------------------------------------------------------------------===//

  BB.declareValue("WeightVar");

  BB.newInstr("AllocActivation").addMember("TypeRef", "Ty").setType("Ty");

  BB.newInstr("DeallocActivation")
      .addOperand("Src", OperandKind::Out)
      .overrideGetter("Src", "AllocActivationInst *getAlloc() const { return "
                             "cast<AllocActivationInst>(getOperand(0).first); "
                             "}")
      .setType("Src->getType()");

  BB.newInstr("Copy")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("Src", OperandKind::In)
      .setType("Src->getType()");

  //===--------------------------------------------------------------------===//
  //                   Convolution / Pool / FC
  //===--------------------------------------------------------------------===//

  BB.newInstr("Convolution")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("Src", OperandKind::In)
      .addOperand("Filter", OperandKind::In)
      .addOperand("Bias", OperandKind::In)
      .addMember("size_t", "Kernel")
      .addMember("size_t", "Stride")
      .addMember("size_t", "Pad")
      .addMember("size_t", "Depth")
      .addGradientInstr({"Src", "Filter"}, {"Dest", "Src", "Filter", "Bias"});

  BB.newInstr("PoolMax")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("Src", OperandKind::In)
      .addOperand("SrcXY", OperandKind::InOut)
      .addMember("size_t", "Kernel")
      .addMember("size_t", "Stride")
      .addMember("size_t", "Pad")
      .addGradientInstr({"Dest", "SrcXY"}, {"Dest", "Src"});

  BB.newInstr("PoolAvg")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("Src", OperandKind::In)
      .addMember("size_t", "Kernel")
      .addMember("size_t", "Stride")
      .addMember("size_t", "Pad")
      .addGradientInstr({"Dest"}, {"Dest", "Src"});

  BB.newInstr("FullyConnected")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("Src", OperandKind::In)
      .addOperand("Filter", OperandKind::In)
      .addOperand("Bias", OperandKind::In)
      .addMember("size_t", "Depth")
      .addGradientInstr({"Src", "Filter"}, {"Dest", "Src", "Filter", "Bias"});

  //===--------------------------------------------------------------------===//
  //                     Normalization
  //===--------------------------------------------------------------------===//

  BB.newInstr("BatchNormalization")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("Src", OperandKind::In)
      .addOperand("Scale", OperandKind::In)
      .addOperand("Bias", OperandKind::In)
      .addOperand("Mean", OperandKind::In)
      .addOperand("Var", OperandKind::In)
      .addMember("size_t", "ChannelIdx")
      .addMember("float", "Epsilon")
      .addMember("float", "Momentum")
      .inplaceOperand({
          "Dest",
          "Src",
      })
      .addGradientInstr({"Src", "Scale", "Mean", "Var"},
                        {"Dest", "Src", "Scale", "Bias"});

  BB.newInstr("LocalResponseNormalization")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("Src", OperandKind::In)
      .addOperand("Scale", OperandKind::In)
      .addMember("size_t", "HalfWindowSize")
      .addMember("float", "Alpha")
      .addMember("float", "Beta")
      .addMember("float", "K")
      .setType("Src->getType()")
      .inplaceOperand({
          "Dest",
          "Src",
      })
      .addGradientInstr({"Dest", "Src", "Scale"}, {"Dest", "Src", "Scale"});

  //===--------------------------------------------------------------------===//
  //                      Loss operations
  //===--------------------------------------------------------------------===//

  BB.newInstr("SoftMax")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("Src", OperandKind::In)
      .addOperand("E", OperandKind::InOut)
      .addOperand("Selected", OperandKind::InOut)
      .inplaceOperand({"Dest", "Src"})
      .addGradientInstr({"Src", "E", "Selected"}, {"Dest", "Src"});

  BB.newInstr("Regression")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("Src", OperandKind::In)
      .addOperand("Expected", OperandKind::InOut)
      .inplaceOperand({"Dest", "Src"})
      .addGradientInstr({"Src", "Expected"}, {"Dest", "Src"});

  //===--------------------------------------------------------------------===//
  //                      Arithmetic
  //===--------------------------------------------------------------------===//

  BB.newInstr("ElementAdd")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("LHS", OperandKind::In)
      .addOperand("RHS", OperandKind::In)
      .inplaceOperand({"Dest", "LHS", "RHS"})
      .addGradientInstr({}, {"Dest", "LHS", "RHS"});

  BB.newInstr("ElementMul")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("LHS", OperandKind::In)
      .addOperand("RHS", OperandKind::In)
      .inplaceOperand({"Dest", "LHS", "RHS"})
      .addGradientInstr({"LHS", "RHS"}, {"Dest", "LHS", "RHS"});

  //===--------------------------------------------------------------------===//
  //                Non-linearities
  //===--------------------------------------------------------------------===//

  BB.newInstr("Relu")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("Src", OperandKind::In)
      .inplaceOperand({
          "Dest",
          "Src",
      })
      .addGradientInstr({"Dest"}, {"Dest", "Src"});

  BB.newInstr("Sigmoid")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("Src", OperandKind::In)
      .inplaceOperand({
          "Dest",
          "Src",
      })
      .addGradientInstr({"Dest"}, {"Dest", "Src"});

  BB.newInstr("Tanh")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("Src", OperandKind::In)
      .inplaceOperand({
          "Dest",
          "Src",
      })
      .addGradientInstr({"Dest"}, {"Dest", "Src"});

  //===--------------------------------------------------------------------===//
  //                Shape transformations
  //===--------------------------------------------------------------------===//

  BB.newInstr("Reshape")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("Src", OperandKind::In)
      .addMember("std::vector<size_t>", "Dims")
      .overrideGetter(
          "Dims", "llvm::ArrayRef<size_t> getDims() const { return Dims_; }")
      .addGradientInstr({}, {"Dest", "Src"});

  BB.newInstr("Transpose")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("Src", OperandKind::In)
      .addMember("std::vector<unsigned>", "Shuffle")
      .overrideGetter(
          "Shuffle",
          "llvm::ArrayRef<unsigned> getShuffle() const { return Shuffle_; }")
      .addGradientInstr({}, {"Dest", "Src"});

  BB.newInstr("Concat")
      .addOperand("Dest", OperandKind::Out)
      .addOperand("LHS", OperandKind::In)
      .addOperand("RHS", OperandKind::In)
      .addMember("size_t", "Dim")
      .addGradientInstr({}, {"Dest", "LHS", "RHS"});

  return 0;
}