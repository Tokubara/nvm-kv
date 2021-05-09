# ---
# jupyter:
#   jupytext:
#     text_representation:
#       extension: .R
#       format_name: light
#       format_version: '1.5'
#       jupytext_version: 1.11.1
#   kernelspec:
#     display_name: R
#     language: R
#     name: ir
# ---

result<-read_csv("result.csv",col_names = c("threadNR","readNR","isSkewed","op"),col_types = list(col_integer(),col_integer(),col_factor(ordered=TRUE),col_double()))

ggplot(result, aes(x=threadNR, y=op, color=isSkewed)) +
  geom_point() + 
  facet_grid(. ~ readNR)

result_32<-read_csv("result_32.csv",col_names = c("threadNR","readNR","isSkewed","op"),col_types = list(col_integer(),col_integer(),col_factor(ordered=TRUE),col_double()))

ggplot(result_32, aes(x=threadNR, y=op, color=isSkewed)) +
  geom_point() + 
  facet_grid(. ~ readNR)

result_32%>%dplyr::filter(isSkewed==1)%>%.$op
